// Worker regressions use the genuine decoder with controlled browser transports.
import assert from "node:assert/strict";
import {setTimeout as delay} from "node:timers/promises";
const initial = new Map(["fetch","WebSocket","location","onmessage","postMessage"].map(k=>[k, Object.getOwnPropertyDescriptor(globalThis,k)]));
const messages=[];
let socketCount=0, lastSocket, end=false, pollCount=0;
class Socket {
  constructor(){lastSocket=this;socketCount++;}
}
const set=(k,v)=>Object.defineProperty(globalThis,k,{configurable:true,writable:true,value:v});
function frame(kind,seq,payload){
  const u=new Uint8Array(16+payload.length),v=new DataView(u.buffer);
  u.set([65,90,83,84]);v.setUint16(4,kind,true);v.setUint16(6,1,true);
  v.setUint32(8,payload.length,true);v.setUint32(12,seq,true);u.set(payload,16);
  return u.buffer;
}
try{
  set("location",{search:"",protocol:"http:",host:"localhost"});set("WebSocket",Socket);
  set("onmessage",undefined);set("postMessage",m=>messages.push(m));
  set("fetch",async(url)=>{
    if(url==="/api/state")return {ok:true,json:async()=>({run_id:"run001",running:!end})};
    assert.match(url,/^\/api\/snap\?/);pollCount++;
    return {status:410,json:async()=>({ended:true})};
  });
  await import("../../demo/viewer_decode.js?worker-regression-websocket");
  await delay(0);assert.equal(socketCount,1);
  const bytes=new Uint8Array(64);const v=new DataView(bytes.buffer);
  v.setUint32(28,24,true);bytes.set([4,4,3,0],32);
  lastSocket.onmessage({data:frame(2,0,bytes)});globalThis.onmessage({data:{kind:"ready"}});
  lastSocket.onmessage({data:frame(2,0,bytes)});globalThis.onmessage({data:{kind:"ready"}});
  assert.equal(messages.filter(m=>m.kind==="snapshot").length,1,"a replay cannot be a new snapshot");
  end=true;lastSocket.onmessage({data:frame(1,2,new TextEncoder().encode(JSON.stringify({event:"end",tick:0})))});
  await lastSocket.onclose();await delay(675);
  assert.equal(socketCount,1,"no reconnect loop after an engine end");
  assert.equal(messages.filter(m=>m.kind==="ended").length,1);
  location.search="?transport=poll";
  await import("../../demo/viewer_decode.js?worker-regression-longpoll");await delay(30);
  assert.equal(pollCount,6,"exactly one terminal fetch per frame type");
  await delay(30);assert.equal(pollCount,6,"no polling after EOF");
  console.log("Terminal transports stopped; repeated sequences produced zero extra observations");
}finally{
  for(const [k,desc] of initial){if(desc)Object.defineProperty(globalThis,k,desc);else delete globalThis[k];}
}

