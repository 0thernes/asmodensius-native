
/* AZST stream and camera math. Shared between the browser worker and Node. */
export const TYPE = Object.freeze({ JSON: 1, CREATURES: 2, STATIC: 3, FIELDS: 4, SHAPES: 5, PICTURE: 6 });
export const VERSION = 1;
export const FRAME_BYTES = 16;
export const HEADER_BYTES = 64;
export const RECORD_BYTES = 24;
export const SHAPE_BYTES = 26 + 48 * 48 * 2;
const textDecoder = new TextDecoder("utf-8", {fatal:true});

function fail(message) { throw new Error(message); }
export function decodeFrame(buffer) {
  if (!(buffer instanceof ArrayBuffer) || buffer.byteLength < FRAME_BYTES) fail("Truncated AZST header");
  const view = new DataView(buffer);
  if (view.getUint32(0, true) !== 0x54535a41) fail("Invalid AZST signature");
  const type = view.getUint16(4, true), version = view.getUint16(6, true);
  const length = view.getUint32(8, true);
  if (version !== VERSION || type < 1 || type > 6) fail("Unsupported AZST frame");
  if (length > 0x7fffffff || length !== buffer.byteLength - FRAME_BYTES) fail("Invalid AZST payload length");
  return {buffer, type, seq:view.getUint32(12,true), length, payload:new DataView(buffer,FRAME_BYTES)};
}
export function decodeJSON(frame) {
  return JSON.parse(textDecoder.decode(new Uint8Array(frame.buffer,16)));
}
export function decodeCreatures(frame) {
  const v = frame.payload;
  if (frame.type !== TYPE.CREATURES || v.byteLength < HEADER_BYTES) fail("Truncated creature header");
  const count = v.getUint32(24,true);
  if (v.getUint32(28,true) !== RECORD_BYTES || v.byteLength !== HEADER_BYTES + count * RECORD_BYTES) fail("Invalid creature record count");
  const bits = [v.getUint8(32),v.getUint8(33),v.getUint8(34)];
  if (bits.some(x=>x>12) || v.getUint8(35)>1) fail("Invalid creature world");
  const n = count * 6;
  return {buffer:frame.buffer, seq:frame.seq, count, tick:v.getBigUint64(0,true), before:v.getBigUint64(8,true), after:v.getBigUint64(16,true),
    dims:bits.map(x=>2**x), wild:v.getUint8(35)===1, tickDelta:v.getUint32(36,true), tickUs:v.getUint32(40,true),
    light:[v.getInt8(44)/127,v.getInt8(45)/127,v.getInt8(46)/127], clades:v.getUint32(48,true), failures:v.getUint32(52,true),
    words:new Uint32Array(frame.buffer,80,n), coords:new Uint16Array(frame.buffer,80,n*2), bytes:new Uint8Array(frame.buffer,80,n*4)};
}
function dimensions(v, offset) {
  const dims=[0,4,8].map(i=>v.getUint32(offset+i,true));
  if (dims.some(x=>x<1 || x>4096 || (x & (x-1))!==0) || dims[0]*dims[1]*dims[2]>2**26) fail("Unsupported grid");
  return dims;
}
export function decodeStatic(frame) {
  const v=frame.payload;
  if (frame.type!==TYPE.STATIC || v.byteLength<32) fail("Truncated terrain header");
  const dims=dimensions(v,0), [gx,gy,gz]=dims, cells=gx*gy*gz, nv=v.getUint32(16,true), base=32+4*nv;
  if(nv>64 || v.getUint32(12,true)!==cells || v.byteLength!==base+cells+4*gx*gy) fail("Invalid terrain layout");
  const vents=Array.from({length:nv},(_,i)=>v.getUint32(32+4*i,true));
  if(vents.some(n=>n>=cells)) fail("Vent outside world");
  // Minimum real world dimensions make this offset four-byte aligned.
  return {dims, vents, periods:[v.getUint32(20,true),v.getUint32(24,true)], stride:v.getUint32(28,true),
    rock:new Uint8Array(frame.buffer,16+base,cells), heights:heightArray(v,base+cells,gx*gy)};
}
function heightArray(v,offset,n) {
  if ((v.byteOffset+offset)%4===0) return new Uint32Array(v.buffer,v.byteOffset+offset,n);
  return Uint32Array.from({length:n},(_,i)=>v.getUint32(offset+4*i,true));
}
export function decodeFields(frame) {
  const v=frame.payload;
  if(frame.type!==TYPE.FIELDS || v.byteLength<32) fail("Truncated fields header");
  const dims=dimensions(v,8), [gx,gy,gz]=dims, n=gx*gy, parts=v.getUint32(20,true);
  if ((parts & ~3)!==0) fail("Unknown field layout");
  const waterBytes=(parts&1)?4*n:0, cellBytes=(parts&2)?4*n*gz:0;
  if (v.byteLength!==32+waterBytes+cellBytes) fail("Invalid field length");
  return {dims, tick:v.getBigUint64(0,true), water:waterBytes?heightArray(v,32,n):null,
    fields:cellBytes?new Uint8Array(frame.buffer,48+waterBytes,cellBytes):null};
}
export function decodeShapes(frame) {
  const v=frame.payload;
  if(frame.type!==TYPE.SHAPES || v.byteLength<4) fail("Truncated shape header");
  const count=v.getUint32(0,true);
  if (v.byteLength!==4+count*SHAPE_BYTES) fail("Invalid shape table");
  const shapes=[];
  for(let i=0;i<count;i++) {
    const off=4+i*SHAPE_BYTES;
    shapes.push({clade:v.getUint32(off,true), body:new Uint8Array(frame.buffer,16+off+4,16),
      elong:v.getUint16(off+20,true)/256, spike:v.getUint16(off+22,true)/256, gmax:v.getUint16(off+24,true)/256,
      base:new Uint8Array(frame.buffer,16+off+26,2304), spikes:new Uint8Array(frame.buffer,16+off+2330,2304)});
  }
  return shapes;
}
export const wrapDelta16 = (from,to) => ((to-from+32768)&65535)-32768;
export function uidAt(s,i) {
  return s.words[i*6+1].toString(16).padStart(8,"0")+s.words[i*6].toString(16).padStart(8,"0");
}
export function findUid(s,uid) {
  if(!s || !/^[0-9a-f]{16}$/i.test(uid)) return -1;
  const hi=parseInt(uid.slice(0,8),16),lo=parseInt(uid.slice(8),16);
  let a=0,b=s.count;
  while(a<b) {
    const i=Math.floor((a+b)/2), h=s.words[i*6+1], l=s.words[i*6];
    if(h<hi || (h===hi && l<lo)) a=i+1; else b=i;
  }
  return a<s.count && s.words[a*6+1]===hi && s.words[a*6]===lo ? a : -1;
}
// Linear merge in ascending unsigned (high,low) order. XYZ+birth flag as integer attributes.
export function mergePrev(prev,next) {
  const out=new Uint16Array(next.count*4);
  let p=0;
  for(let i=0;i<next.count;i++) {
    const hi=next.words[i*6+1],lo=next.words[i*6];
    while(prev && p<prev.count) {
      const ph=prev.words[p*6+1],pl=prev.words[p*6];
      if(ph>hi || (ph===hi && pl>=lo)) break;
      p++;
    }
    const match=prev && p<prev.count && prev.words[p*6+1]===hi && prev.words[p*6]===lo;
    const src=match?prev:next, k=(match?p:i)*12+4;
    out[i*4]=src.coords[k]; out[i*4+1]=src.coords[k+1]; out[i*4+2]=src.coords[k+2];
    out[i*4+3]=prev && !match?1:0;
  }
  return out;
}
const dot=(a,b)=>a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
// Match the literal used by the engine proof renderer (view/az_view.c).
export const FOV_TAN=Math.fround(0.36397);
export function orbitCamera(box,yaw,pitch,distance,width,height,target=box.map(x=>x/2)) {
  pitch=Math.min(1.55,Math.max(-1.55,pitch));
  const cp=Math.cos(pitch),sp=Math.sin(pitch),cy=Math.cos(yaw),sy=Math.sin(yaw);
  const forward=[-cp*cy,-cp*sy,-sp],right=[-sy,cy,0],up=[-sp*cy,-sp*sy,cp];
  const focal=0.5*height/FOV_TAN;
  if(!(distance>0)) {
    const r=0.5*Math.hypot(...box); let lo=.25*r,hi=16*r;
    const corners=Array.from({length:8},(_,i)=>box.map((v,k)=>((i>>k)&1)*v-target[k]));
    for(let it=0;it<28;it++) {
      const mid=.5*(lo+hi);
      const fits=corners.every(v=>{
        const z=dot(v,forward)+mid;
        return z>0.05 && Math.abs(dot(v,right)*focal)<=.47*width*z && Math.abs(dot(v,up)*focal)<=.47*height*z;
      });
      if(fits) hi=mid; else lo=mid;
    }
    distance=hi;
  }
  return {eye:target.map((v,k)=>v-distance*forward[k]), forward,right,up,distance,focal,width,height};
}
export function projectPoint(camera,p) {
  const d=p.map((v,i)=>v-camera.eye[i]), z=dot(d,camera.forward);
  return [camera.width/2+camera.focal*dot(d,camera.right)/z,camera.height/2-camera.focal*dot(d,camera.up)/z,z];
}
export function quantile(a,q) {
  if(!a.length) return null;
  const sorted=Array.from(a).sort((x,y)=>x-y);
  return sorted[Math.min(sorted.length-1,Math.max(0,Math.ceil(q*sorted.length)-1))];
}
