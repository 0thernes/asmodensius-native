
import {TYPE,decodeFrame,decodeCreatures,decodeJSON,mergePrev} from "./viewer_core.js";

let current=null, previousPositions=null, currentSeq=null, camera=null;
let nearTimer=null, sentAt=0, pageBusy=false, latest=null;
const clock=()=>performance.timeOrigin+performance.now();
const cursors=new Map();
let runID=null, ended=false;
const report=(status)=>postMessage({kind:"status",status});
function transferLatest(){
  if(pageBusy || !latest) return;
  const item=latest;
  latest=null; pageBusy=true;
  const buffer=item.snapshot.buffer.slice(0);
  const positions=item.positions.slice();
  postMessage({kind:"snapshot",buffer,prev:positions.buffer,arrived:item.arrived},[buffer,positions.buffer]);
}
function handle(buffer){
  const arrived=clock();
  const frame=decodeFrame(buffer);
  const prior=cursors.get(frame.type);
  if(prior!==undefined && ((frame.seq-prior)>>>0)>=0x80000000)return;
  if(prior===frame.seq)return;
  cursors.set(frame.type,frame.seq);
  if(frame.type===TYPE.CREATURES){
    const snapshot=decodeCreatures(frame);
    const reset=!current || snapshot.tick<current.tick || snapshot.dims.some((n,i)=>n!==current.dims[i]);
    previousPositions=mergePrev(reset?null:current,snapshot);
    current=snapshot; currentSeq=frame.seq;
    latest={snapshot,positions:previousPositions,arrived};
    transferLatest();
    if(performance.now()-lastLegend>1000){
      const counts=new Map();
      for(let i=5;i<snapshot.words.length;i+=6){const id=snapshot.words[i];counts.set(id,(counts.get(id)||0)+1);}
      const top=[...counts].sort((a,b)=>b[1]-a[1]||a[0]-b[0]).slice(0,5);
      postMessage({kind:"legend",tick:String(snapshot.tick),top});lastLegend=performance.now();
    }
    scheduleNear();
  } else {
    if(frame.type===TYPE.JSON && decodeJSON(frame).event==="end")ended=true;
    postMessage({kind:"frame",buffer},[buffer]);
  }
}
// Far bodies always render directly from the packed stream. Only nearby, visible meshes
// are gathered into the extra triangle pass. Selection and packing happen off the UI thread.
function scheduleNear(){
  if(!nearTimer && camera && current) nearTimer=setTimeout(updateNear,Math.max(0,100-(performance.now()-sentAt)));
}
function updateNear(){
  nearTimer=null; sentAt=performance.now();
  if(!current || !camera) return;
  const {eye,forward,right,up,focal,width,height}=camera;
  const dims=current.dims, half=dims.map(x=>x/2);
  const boxDepth=half.reduce((s,h,i)=>s+(h-eye[i])*forward[i]-h*Math.abs(forward[i]),0);
  let count=0;
  const ids=[],close=[];
  const detailBudget=64;
  // Min-heap with the worst retained candidate at its root.
  const worse=(a,b)=>a.pixels<b.pixels || (a.pixels===b.pixels && a.index>b.index);
  function consider(index,pixels){
    const entry={index,pixels};
    if(close.length<detailBudget){
      let child=close.length;
      close.push(entry);
      while(child>0){
        const parent=(child-1)>>1;
        if(!worse(entry,close[parent]))break;
        close[child]=close[parent];child=parent;
      }
      close[child]=entry;
    }else if(worse(close[0],entry)){
      let parent=0;
      while(2*parent+1<close.length){
        let child=2*parent+1;
        if(child+1<close.length && worse(close[child+1],close[child]))child++;
        if(!worse(close[child],entry))break;
        close[parent]=close[child];parent=child;
      }
      close[parent]=entry;
    }
  }
  if(boxDepth<.5*focal/3){
    for(let i=0;i<current.count;i++){
      const k=i*12+4, x=current.coords[k]*dims[0]/65536-eye[0], y=current.coords[k+1]*dims[1]/65536-eye[1], z=current.coords[k+2]*dims[2]/65536-eye[2];
      const depth=x*forward[0]+y*forward[1]+z*forward[2],r=(current.bytes[i*24+16]+1)/32;
      if(depth<.05 || depth>r*focal/3) continue;
      const px=x*right[0]+y*right[1]+z*right[2],py=x*up[0]+y*up[1]+z*up[2];
      // The native bound is (1.8 + 0.8) * pow(2.5, 2/3) radii.
      // A close-up can intersect the viewport while its center is outside.
      const margin=r*(camera.detail?4.8:2.5);
      if(Math.abs(px)>depth*width/(2*focal)+margin || Math.abs(py)>depth*height/(2*focal)+margin) continue;
      // Rank only visible bodies, in this worker. Keep the per-pixel pass bounded.
      if(camera.detail){
        const flags=current.bytes[i*24+19],radius=(flags&2)?r*.5:r;
        const pixels=radius*focal/depth;
        if(pixels>=6 && depth>=.05+2*radius)consider(count,pixels);
      }
      ids.push(i); count++;
    }
  }
  const packed=new Uint8Array(count*24), prev=new Uint16Array(count*4), indices=Uint32Array.from(ids);
  for(let n=0;n<count;n++){
    const i=indices[n];
    packed.set(current.bytes.subarray(i*24,i*24+24),n*24);
    prev.set(previousPositions.subarray(i*4,i*4+4),n*4);
  }
  // Stable UID order resolves ties in projected radius.
  const detailIDs=Uint32Array.from(close,entry=>entry.index);
  postMessage({kind:"near",seq:currentSeq,packed:packed.buffer,prev:prev.buffer,ids:indices.buffer,
    detailIDs:detailIDs.buffer,cameraKey:camera.key},[packed.buffer,prev.buffer,indices.buffer,detailIDs.buffer]);
}
onmessage=event=>{
  if(event.data.kind==="ready"){pageBusy=false;transferLatest();}
  if(event.data.kind==="camera"){camera=event.data.camera;scheduleNear();}
};

let failures=0, lastLegend=-Infinity;
async function serverState(){
  const response=await fetch("/api/state",{cache:"no-store"});
  if(!response.ok)throw Error("State HTTP "+response.status);
  const info=await response.json();
  if(runID!==null && info.run_id!==runID){
    // A new run can have identical sequence numbers, tick and dimensions.
    // Rebuild the page so its GL caches and selections cannot cross worlds.
    postMessage({kind:"restart"});throw Error("World changed");
  }
  runID=info.run_id;
  return info;
}
async function poll(type){
  while(true){
    try{
      const after=cursors.has(type)?cursors.get(type):-1;
      const href="/api/snap?type="+type+"&after="+after+(runID?"&run_id="+encodeURIComponent(runID):"");
      const response=await fetch(href,{cache:"no-store"});
      if(response.status===409){postMessage({kind:"restart"});return;}
      if(response.status===410){
        const terminal=await response.json();
        postMessage({kind:"ended",error:terminal.error});return;
      }
      if(response.status===204)continue;
      if(!response.ok) throw Error("HTTP "+response.status);
      handle(await response.arrayBuffer());
    } catch(error){
      report("Long poll: "+error.message);
      await new Promise(r=>setTimeout(r,1200));
    }
  }
}
function connect(){
  const socket=new WebSocket((location.protocol==="https:"?"wss://":"ws://")+location.host+"/ws");
  socket.binaryType="arraybuffer";
  socket.onopen=()=>{report("WebSocket connected");};
  socket.onmessage=event=>{
    try{handle(event.data);failures=0;}catch(error){report("Invalid stream: "+error.message);socket.close();}
  };
  socket.onclose=async()=>{
    try{
      const info=await serverState();
      if(ended || !info.running){
        postMessage({kind:"ended",error:info.error});return;
      }
    }catch{}
    if(++failures<3){report("Reconnecting");setTimeout(connect,650);}
    else startPoll();
  };
}
function startPoll(){report("Long poll connected");for(const type of Object.values(TYPE))poll(type);}
async function openTransport(){
  try{await serverState();}catch{}
  if(new URLSearchParams(location.search).get("transport")==="poll")startPoll();else connect();
}
openTransport();
