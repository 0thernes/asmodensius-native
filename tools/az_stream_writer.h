/* The writer owns all queued buffers except the one reusable creature block.
 * No writer payload retains a pointer to the simulation. */
typedef struct {
    u32 id;
    u8 body[16];
} ShapeKey;
typedef struct {
    AzWorld *scene;
    u32 width, height;
    float yaw, pitch, dist;
    float target[3];
    bool have_target;
} ProofScene;
typedef struct {
    u16 type;
    u32 bytes;
    void *data;
} StreamJob;
enum { STREAM_QUEUE = 64 };
typedef struct {
    HANDLE output, ready, thread;
    CRITICAL_SECTION lock;
    StreamJob jobs[STREAM_QUEUE];
    u32 head, count, serial[7];
    bool closing;
    volatile LONG failed, snap_busy, proof_busy, fields_busy;
} StreamWriter;

static bool stream_write(HANDLE h, const void *data, size_t size)
{
    const u8 *p = data;
    while (size) {
        DWORD wrote = 0;
        DWORD part = size > 1048576 ? 1048576 : (DWORD)size;
        if (!WriteFile(h, p, part, &wrote, NULL) || wrote == 0) return false;
        p += wrote; size -= wrote;
    }
    return true;
}

static bool stream_prefix(StreamWriter *q, u16 type, u32 bytes)
{
    AzStreamFrame h = {AZST_MAGIC, type, AZST_VERSION, bytes, q->serial[type]++};
    return stream_write(q->output, &h, sizeof h);
}

static bool stream_shapes_write(StreamWriter *q, const ShapeKey *keys, u32 n)
{
    /* A valid frame stays under the wire limit even for unusually many clades. */
    do {
        u32 batch = az_min_u32(n, 1024);
        if (!stream_prefix(q, AZST_SHAPES, 4 + batch * AZST_SHAPE_BYTES) ||
            !stream_write(q->output, &batch, 4)) return false;
        for (u32 i = 0; i < batch; i++) {
            AzShapeTable table;
            az_view_shape_table(keys[i].body, &table);
            _Static_assert(sizeof table == AZST_SHAPE_BYTES - sizeof(ShapeKey), "shape table");
            if (!stream_write(q->output, &keys[i], sizeof keys[i]) ||
                !stream_write(q->output, &table, sizeof table)) return false;
        }
        keys += batch; n -= batch;
    } while (n);
    return true;
}

static bool stream_proof_write(StreamWriter *q, const ProofScene *proof)
{
    AzView *v = az_view_create(proof->width, proof->height);
    if (!v) return false;
    az_view_camera(v, proof->yaw, proof->pitch, proof->dist, proof->have_target ? proof->target : NULL);
    az_view_render(v, proof->scene);
    size_t length = 0;
    u8 *png = az_png_encode(az_view_pixels(v), proof->width, proof->height, &length);
    bool ok = png && length <= AZST_MAX_BYTES && stream_prefix(q, AZST_PICTURE, (u32)length) &&
              stream_write(q->output, png, length);
    free(png);
    az_view_destroy(v);
    return ok;
}

static void stream_dispose(StreamWriter *q, StreamJob job)
{
    if (job.type == AZST_CREATURES) {
        InterlockedExchange(&q->snap_busy, 0);
        return;
    }
    if (job.type == AZST_PICTURE) {
        ProofScene *p = job.data;
        az_view_capture_free(p->scene);
        InterlockedExchange(&q->proof_busy, 0);
    }
    if (job.type == AZST_FIELDS) InterlockedExchange(&q->fields_busy, 0);
    free(job.data);
}

static DWORD WINAPI stream_worker(void *arg)
{
    StreamWriter *q = arg;
    for (;;) {
        WaitForSingleObject(q->ready, INFINITE);
        EnterCriticalSection(&q->lock);
        if (!q->count) {
            bool closing = q->closing;
            ResetEvent(q->ready);
            LeaveCriticalSection(&q->lock);
            if (closing) break;
            continue;
        }
        StreamJob job = q->jobs[q->head];
        q->head = (q->head + 1u) % STREAM_QUEUE;
        q->count--;
        LeaveCriticalSection(&q->lock);
        bool ok = true;
        if (!InterlockedCompareExchange(&q->failed, 0, 0)) {
            if (job.type == AZST_SHAPES) ok = stream_shapes_write(q, job.data, job.bytes);
            else if (job.type == AZST_PICTURE) ok = stream_proof_write(q, job.data);
            else ok = stream_prefix(q, job.type, job.bytes) && stream_write(q->output, job.data, job.bytes);
        }
        stream_dispose(q, job);
        if (!ok) InterlockedExchange(&q->failed, 1);
    }
    return 0;
}

static bool stream_writer_start(StreamWriter *q)
{
    q->output = GetStdHandle(STD_OUTPUT_HANDLE);
    InitializeCriticalSection(&q->lock);
    q->ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (q->ready) q->thread = CreateThread(NULL, 0, stream_worker, q, 0, NULL);
    return q->thread != NULL;
}

static u32 stream_queue_free(StreamWriter *q)
{
    EnterCriticalSection(&q->lock);
    u32 n = STREAM_QUEUE - q->count;
    LeaveCriticalSection(&q->lock);
    return n;
}

static bool stream_enqueue(StreamWriter *q, u16 type, void *data, u32 size)
{
    if (!data) return false;
    EnterCriticalSection(&q->lock);
    if (q->count == STREAM_QUEUE) {
        LeaveCriticalSection(&q->lock);
        stream_dispose(q, (StreamJob){type, size, data});
        return false;
    }
    q->jobs[(q->head + q->count) % STREAM_QUEUE] = (StreamJob){type, size, data};
    q->count++;
    SetEvent(q->ready);
    LeaveCriticalSection(&q->lock);
    return true;
}

static bool stream_json(StreamWriter *q, const char *text)
{
    size_t len = strlen(text);
    void *copy = malloc(len);
    if (copy) memcpy(copy, text, len);
    return stream_enqueue(q, AZST_JSON, copy, (u32)len);
}

static bool stream_writer_close(StreamWriter *q)
{
    if (q->thread) {
        EnterCriticalSection(&q->lock);
        q->closing = true;
        SetEvent(q->ready);
        LeaveCriticalSection(&q->lock);
        /* Disconnects must never trap the engine on a full anonymous pipe. */
        if (WaitForSingleObject(q->thread, 15000) == WAIT_TIMEOUT) {
            InterlockedExchange(&q->failed, 1);
            /* The worker may still be rendering when cancellation is first
             * requested, then enter its blocking write later. Retry until
             * it has released all queued buffers and exited. */
            do {
                CancelSynchronousIo(q->thread);
            } while (WaitForSingleObject(q->thread, 50) == WAIT_TIMEOUT);
        }
        CloseHandle(q->thread);
    }
    if (q->ready) CloseHandle(q->ready);
    DeleteCriticalSection(&q->lock);
    return !q->failed;
}
