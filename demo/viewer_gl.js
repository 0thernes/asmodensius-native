
import {TYPE,VERSION,decodeFrame,decodeCreatures,decodeStatic,decodeFields,decodeShapes,decodeJSON,orbitCamera,findUid,uidAt,quantile} from "./viewer_core.js";

const $=id=>document.getElementById(id), canvas=$("world");
const state={snapshot:null,previous:null,near:null,terrain:null,fields:null,shapes:new Map(),gpu:"",selected:null,shapeMissing:0,closeCount:0,proofURL:null};
const view={yaw:.6,pitch:.5,distance:0,mode:"orbit",eye:null,speed:8,focal:null};
let gl=null,renderer=null,worker=null,lastRAF=0,lastCamera="",lastNearSend=0,latestArrival=0,snapInterval=100,measure=null;
let frameTimes=[],arrivals=[],latestMeta=null,engineEnded=false,lost=false,paused=false,dirty=true,lastUI=0,ageAtArrival=null;
const keys=new Set(), ageByTick=new Map();
const sceneCamera=()=>state.snapshot?state.snapshot.dims:(state.terrain?.dims||[64,64,32]);
const issue=message=>{$("error").textContent=message;};
const hex=x=>x.toString(16).padStart(16,"0");
const nowAbs=()=>performance.timeOrigin+performance.now();
const sameDims=(a,b)=>a&&b&&a.every((v,i)=>v===b[i]);
const setText=(id,value)=>{$(id).textContent=value;};
const safeJSON=value=>JSON.stringify(value,(_,v)=>typeof v==="bigint"?String(v):v);
async function post(path,body){
  const response=await fetch(path,{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(body)});
  const result=await response.json();
  if(!response.ok || result.ok===false) throw Error(result.error||("HTTP "+response.status));
  return result;
}
const command=cmd=>post("/api/cmd",{cmd});
function camera(){
  const box=sceneCamera(), s=state.snapshot;
  let target=box.map(n=>n/2);
  if(view.mode==="follow" && s && state.selected){
    const i=findUid(s,state.selected);
    if(i>=0) target=box.map((n,k)=>s.coords[i*12+4+k]*n/65536);
  }
  const c=orbitCamera(box,view.yaw,view.pitch,view.mode==="fly"?1:view.distance,canvas.width,canvas.height,target);
  if(view.mode==="fly" && view.eye)c.eye=view.eye.slice();
  return c;
}
let initialCloseCamera=new URLSearchParams(location.search).get("detail")==="close";
if(initialCloseCamera)$("detail").value="close";
const precision="precision highp float; precision highp int;\n";
const header="#version 300 es\n"+precision;
const projection=`uniform vec3 eye, forward, rightDir, upDir, dims, sun;
uniform vec2 viewport;
uniform float focal, alpha, birthFade;
vec4 project(vec3 p) {
 vec3 d=p-eye; float z=dot(d,forward);
 return vec4(dot(d,rightDir)*2.0*focal/viewport.x,dot(d,upDir)*2.0*focal/viewport.y,z*1.000002-0.1000001,z);
}
`;
function compile(gl,kind,source){
  const shader=gl.createShader(kind); gl.shaderSource(shader,header+source); gl.compileShader(shader);
  if(!gl.getShaderParameter(shader,gl.COMPILE_STATUS))throw Error(gl.getShaderInfoLog(shader));
  return shader;
}
function program(gl,v,f){
  const p=gl.createProgram(),vs=compile(gl,gl.VERTEX_SHADER,v),fs=compile(gl,gl.FRAGMENT_SHADER,f);
  gl.attachShader(p,vs); gl.attachShader(p,fs); gl.linkProgram(p);gl.deleteShader(vs);gl.deleteShader(fs);
  if(!gl.getProgramParameter(p,gl.LINK_STATUS))throw Error(gl.getProgramInfoLog(p));
  return p;
}
const bodyVert=`layout(location=0) in uvec3 pos16; layout(location=1) in uvec2 angles;
layout(location=2) in uvec4 look; layout(location=3) in uvec4 prev16;
layout(location=4) in vec3 normal0; layout(location=5) in uint original;
layout(location=6) in vec4 shape; layout(location=7) in uvec2 uid;
#ifndef NEAR_MESH
layout(location=8) in uint detailed;
#endif
uniform highp sampler2D atlas; uniform highp usampler2D surfaceWater; uniform bool haveWater; uniform int tilesPerRow; uniform highp uvec2 chosen; uniform bool hasChosen;
out vec3 baseColor; out vec3 normalW; flat out uint indexID; flat out uint pattern;
out float fog; uniform bool immersed;
vec3 colorWheel(float h){return mix(vec3(.22),clamp(abs(fract(h+vec3(0,.667,.333))*6.-3.)-1.,0.,1.),.81);}
vec2 oct(vec3 n){n/=abs(n.x)+abs(n.y)+abs(n.z);return (n.z>=0.?n.xy:(1.-abs(n.yx))*sign(n.xy))*.5+.5;}
void main(){
 ivec3 delta=ivec3(pos16)-ivec3(prev16.xyz);
 delta.xy=((delta.xy+32768)&ivec2(65535))-32768;
 vec3 p=vec3(prev16.xyz)+alpha*vec3(delta);
 p.xy=mod(p.xy,65536.); p=p*dims/65536.;
 float radius=(float(look.x)+1.)/32.;
 if(prev16.w!=0u)radius*=max(birthFade,.05);
 float hueF=look.y!=0u?float(look.y)/256.:fract(float((uid.x*2654435761u)^(uid.y*2246822519u))/4294967296.);
 baseColor=sqrt(max(colorWheel(hueF),vec3(.02))); pattern=look.z;
 if(hasChosen && all(equal(uid,chosen)))baseColor=vec3(1.,.98,.55);
 float z=dot(p-eye,forward);
 #ifdef NEAR_MESH
  vec3 n=normal0;
  float scale=1.,e=1.;
  if(shape.x>=0.) {
   int idx=int(shape.x);
   vec2 tileOrigin=vec2(idx%tilesPerRow,idx/tilesPerRow)*48.;
   vec2 tile=texture(atlas,(tileOrigin+.5+47.*oct(n))/vec2(textureSize(atlas,0))).rg;
   scale=.35+tile.r*1.45+shape.z*tile.g; e=shape.y;
  }
  float yaw=float(angles.x)*6.2831853/256.;
  float pitch=float(angles.y>127u?int(angles.y)-256:int(angles.y))*1.5707963/127.;
  vec3 axis=vec3(cos(pitch)*cos(yaw),cos(pitch)*sin(yaw),sin(pitch));
  vec3 left=vec3(-sin(yaw),cos(yaw),0.);vec3 top=cross(axis,left);
  p+=(n.x*e*axis+n.y*left+n.z*top)*scale*radius/pow(e,1./3.);
  normalW=normalize(n.x/e*axis+n.y*left+n.z*top);indexID=original+1u;
 #else
  normalW=vec3(0,0,1); indexID=uint(gl_VertexID)+1u;
  gl_PointSize=clamp(2.*radius*focal/max(.05,z),1.15,96.);
 #endif
 gl_Position=project(p);
 #ifdef NEAR_MESH
 if(shape.w>0.)gl_Position=vec4(0.,0.,2.,1.);
 #endif
 #ifndef NEAR_MESH
 if(detailed!=0u)gl_Position=vec4(0.,0.,2.,1.);
 #endif
 bool below=false;
 if(haveWater){ivec2 at=(ivec2(floor(p.xy))%ivec2(dims.xy)+ivec2(dims.xy))%ivec2(dims.xy);below=p.z<float(texelFetch(surfaceWater,at,0).r)/256.;}
 fog=(immersed || below)?clamp(z/(.35*length(dims)),0.,.85):0.;
}`;
function bodyFrag(mesh,pick){
  return (mesh?"#define NEAR_MESH\n":"")+(pick?"#define PICK\n":"")+`
in vec3 baseColor;in vec3 normalW;in float fog;flat in uint indexID;flat in uint pattern;
uniform vec3 sun,forward,rightDir,upDir;
#ifdef PICK
layout(location=0) out highp uint color;
#else
out vec4 color;
#endif
void main(){
 vec3 n=normalize(normalW);
 #ifndef NEAR_MESH
 vec2 a=vec2(gl_PointCoord.x,1.-gl_PointCoord.y)*2.-1.;
 float r=dot(a,a);if(r>1.) discard;
 n=normalize(a.x*rightDir+a.y*upDir-sqrt(max(0.,1.-r))*forward);
 #endif
 #ifdef PICK
 color=indexID;
 #else
 float light=.45+.55*max(0.,dot(sun,n));
 float stripe=.96+.04*cos(float(pattern%11u+1u)*n.z*4.);
 color=vec4(mix(baseColor*light*stripe,vec3(.10,.28,.29),fog),1);
 #endif
}`;
}
// The reference renderer projects the body center in perspective, then marches
// parallel rays through the native octahedral radius field for that body.
const closeVert = `
layout(location=0) in uvec3 pos16;
layout(location=1) in uvec2 angles;
layout(location=2) in uvec4 look;
layout(location=3) in uvec4 prev16;
layout(location=5) in uint original;
layout(location=6) in vec4 shape;
layout(location=7) in uvec2 uid;
layout(location=9) in uvec4 material;
layout(location=10) in uint sheen;
uniform uvec2 chosen;
uniform bool hasChosen;
flat out vec4 plan;
flat out vec3 center;
flat out vec3 axis, leftDir, topDir;
flat out uint indexID, flags, shine;
flat out uvec4 materialBytes;
flat out uint selectedBody;
out vec2 offsetWorld;
void main(){
  ivec3 delta=ivec3(pos16)-ivec3(prev16.xyz);
  delta.xy=((delta.xy+32768)&ivec2(65535))-32768;
  vec3 p=vec3(prev16.xyz)+alpha*vec3(delta);
  p.xy=mod(p.xy,65536.);
  p=p*dims/65536.;
  float radius=(float(look.x)+1.)/32.;
  if((look.w&2u)!=0u)radius*=.5;
  if(prev16.w!=0u)radius*=max(birthFade,.05);
  float depth=dot(p-eye,forward);
  bool passive=(look.w&130u)!=0u;
  float elong=passive?1.:shape.y;
  float rq=radius/pow(elong,1./3.);
  float bound=passive?1.:shape.w;
  plan=vec4(shape.x,elong,shape.z,rq);
  center=vec3(depth,rq*bound,focal/max(depth,.05));
  float yaw=float(angles.x)*6.2831853/256.;
  float pitch=float(angles.y>127u?int(angles.y)-256:int(angles.y))*1.5707963/127.;
  axis=vec3(cos(pitch)*cos(yaw),cos(pitch)*sin(yaw),sin(pitch));
  leftDir=vec3(-sin(yaw),cos(yaw),0.);
  topDir=cross(axis,leftDir);
  indexID=original+1u;
  flags=look.w;
  materialBytes=material;
  shine=sheen;
  selectedBody=(hasChosen && all(equal(uid,chosen)))?1u:0u;
  vec2 corner=vec2((gl_VertexID&1)==0?-1.:1.,(gl_VertexID&2)==0?-1.:1.);
  offsetWorld=corner*center.y*elong;
  gl_Position=project(p+rightDir*offsetWorld.x+upDir*offsetWorld.y);
  if(shape.w<=0. || depth<.05+2.*radius)gl_Position=vec4(0.,0.,2.,1.);
}`;

function closeFrag(pick){
  return (pick?"#define PICK\n":"")+projection+`
flat in vec4 plan;
flat in vec3 center, axis, leftDir, topDir;
flat in uint indexID, flags, shine;
flat in uvec4 materialBytes;
flat in uint selectedBody;
in vec2 offsetWorld;
uniform highp sampler2D atlas;
uniform int tilesPerRow;
#ifdef PICK
layout(location=0) out highp uint color;
#else
out vec4 color;
#endif
vec3 bodyFrame(vec3 v){return vec3(dot(v,axis)/plan.y,dot(v,leftDir),dot(v,topDir));}
float field(vec3 q){
  float l=dot(abs(q),vec3(1.));
  if(l<1e-20)return -plan.w;
  vec2 o=q.xy/l;
  if(q.z<0.)o=(1.-abs(o.yx))*mix(vec2(-1.),vec2(1.),greaterThanEqual(o,vec2(0.)));
  int id=int(plan.x);
  vec2 origin=vec2(id%tilesPerRow,id/tilesPerRow)*48.;
  vec2 at=(origin+.5+(o*.5+.5)*47.)/vec2(textureSize(atlas,0));
  vec2 sampleRG=textureLod(atlas,at,0.).rg;
  return length(q)-plan.w*(.35+1.45*sampleRG.r+plan.z*sampleRG.g);
}
float patternShade(uint pat,vec3 d,bool passive){
  uint type=pat>>6u;
  float contrast=.25+.07*float(pat&7u);
  if(passive)return 1.-contrast*(type==0u?.25:.5);
  float f=(2.+float((pat>>3u)&7u))*3.14159265;
  float k=.5+.5*d.z;
  if(type==1u)k=.5+2.*sin(f*d.x);
  if(type==2u){vec3 s=sin(f*d);k=.5+6.*(.2-s.x*s.y*s.z);}
  if(type==3u)k=.5+2.*sin(f*d.y);
  return 1.-contrast*(1.-clamp(k,0.,1.));
}
void main(){
  bool passive=(flags&130u)!=0u;
  vec3 n,dir=vec3(0.,0.,1.);
  float hit;
  if(passive){
    vec2 a=offsetWorld/plan.w;
    float t=1.-dot(a,a);
    if(t<0.)discard;
    hit=-sqrt(t)*plan.w;
    n=a.x*rightDir+a.y*upDir-sqrt(t)*forward;
  }else{
    vec3 origin=bodyFrame(rightDir*offsetWorld.x+upDir*offsetWorld.y);
    vec3 ray=bodyFrame(forward);
    float a=dot(ray,ray),b=dot(origin,ray);
    float disc=b*b-a*(dot(origin,origin)-center.y*center.y);
    if(disc<=0.)discard;
    float t0=(-b-sqrt(disc))/a,t1=(-b+sqrt(disc))/a;
    int steps=clamp(int((t1-t0)*center.z)+3,6,24);
    float lo=t0,hi=t0;
    bool found=false;
    for(int i=1;i<=steps;i++){
      hi=mix(t0,t1,float(i)/float(steps));
      if(field(origin+hi*ray)<=0.){found=true;break;}
      lo=hi;
    }
    if(!found)discard;
    for(int j=0;j<5;j++){
      float mid=.5*(lo+hi);
      if(field(origin+mid*ray)<=0.)hi=mid;else lo=mid;
    }
    hit=hi;
#ifndef PICK
    vec3 q=origin+hit*ray;
    dir=normalize(q);
    float eps=.03*plan.w;
    vec3 grad=vec3(field(q+vec3(eps,0,0)),field(q+vec3(0,eps,0)),field(q+vec3(0,0,eps)))-field(q);
    n=normalize(grad.x/plan.y*axis+grad.y*leftDir+grad.z*topDir);
#endif
  }
  float depth=center.x+hit;
  if(depth<=.05)discard;
  gl_FragDepth=.5+.5*(1.000002-.1000001/depth);
#ifdef PICK
  color=indexID;
#else
  float hue=float(materialBytes.x)/256.;
  float sat=.30+.70*float(materialBytes.y)/255.;
  vec3 wheel=clamp(abs(fract(hue+vec3(0.,2./3.,1./3.))*6.-3.)-1.,0.,1.);
  vec3 rgb=.8*mix(vec3(1.),wheel,sat);
  if(selectedBody!=0u)rgb=vec3(1.,.98,.55);
  vec3 lamp=vec3(.35634832,.25453451,.89087081);
  float glow=float(materialBytes.w)/255.;
  float spec=.9*float(shine)/255.*pow(max(0.,dot(n,normalize(lamp-forward))),32.);
  float dim=(flags&2u)!=0u?.40:((flags&128u)!=0u?.65:1.);
  float lum=dim*patternShade(materialBytes.z,dir,passive)*(.22+.78*max(0.,dot(n,lamp))+.7*glow*glow);
  float diam=length(dims),dist=length(dims*.5-eye);
  float fog=.5*clamp((depth-dist+.5*diam)/(diam+1e-6),0.,1.);
  color=vec4(sqrt(clamp(mix(rgb*lum+dim*spec,vec3(.008,.012,.030),fog),0.,1.)),1.);
#endif
}`;
}

const landVert=projection+`
uniform highp usampler2D groundTex,waterTex;
uniform sampler2D terrainColor;
uniform int layerKind, sliceZ;
layout(location=0) in vec3 vertex;layout(location=1) in uint cellId;
out vec3 point; out vec4 tint;
const vec3 cube[8]=vec3[8](vec3(0,0,0),vec3(1,0,0),vec3(0,1,0),vec3(1,1,0),vec3(0,0,1),vec3(1,0,1),vec3(0,1,1),vec3(1,1,1));
void main(){
 ivec2 d=ivec2(dims.xy);
 if(layerKind==3 || layerKind==4){
  int id=int(cellId),z=id/(d.x*d.y),xy=id-z*d.x*d.y; vec3 origin=vec3(xy%d.x,xy/d.x,z);
  point=origin+(layerKind==3?cube[gl_VertexID]:vec3(.5,.5,1.));
  tint=layerKind==3?vec4(.30,.39,.28,1.):vec4(vertex,1.);
  gl_PointSize=9.;
 } else if(layerKind==5){point=vertex;tint=vec4(.65,.82,.73,.6);}
 else {
  int k=gl_VertexID; ivec2 cell=ivec2(gl_InstanceID%d.x,gl_InstanceID/d.x)+ivec2(k==1 || k==3 || k==4 ? 1:0,k==2 || k==4 || k==5 ? 1:0);
  ivec2 texel=cell%d;
  float z=float(texelFetch(groundTex,texel,0).r)/256.;
  if(layerKind==1)z=float(texelFetch(waterTex,texel,0).r)/256.;
  if(layerKind==2)z=float(sliceZ)+.5;
  point=vec3(cell,z);tint=texelFetch(terrainColor,texel,0);
 }
 gl_Position=project(point);
}`;
function landFragment(pick){
  return (pick?"#define PICK\n":"")+`in vec3 point;in vec4 tint;uniform vec3 sun;uniform int layerKind;
uniform bool gridOn,immersed;uniform int sliceMode;uniform vec3 eye,dims;
#ifdef PICK
layout(location=0)out highp uint color;
#else
out vec4 color;
#endif
void main(){
 if(layerKind==1 && point.z<=.0001)discard;
 #ifdef PICK
 color=0u;
 #else
 vec3 normal=normalize(cross(dFdx(point),dFdy(point)));
 float l=.58+.42*max(0.,dot(normal,sun));
 vec3 c=mix(vec3(.20,.28,.24),vec3(.43,.47,.27),tint.r)*l;float opacity=1.;
 if(layerKind==0 && gridOn){
  vec2 e=abs(fract(point.xy-.5)-.5)/max(fwidth(point.xy),vec2(.0001));c*=mix(.70,1.,clamp(min(e.x,e.y),0.,1.));
 }
 if(layerKind==1){c=vec3(.13,.48,.51)*(.8+.2*l);opacity=.46;}
 if(layerKind==2){float f=sliceMode==0?tint.g:sliceMode==1?tint.b:tint.a;c=mix(vec3(.12,.26,.36),vec3(.95,.68,.28),f);opacity=.72;}
 if(layerKind==3)c=tint.rgb*l;
 if(layerKind==4 || layerKind==5){c=tint.rgb;opacity=tint.a;}
 if(immersed)c=mix(c,vec3(.10,.28,.29),clamp(length(eye-point)/(.35*length(dims)),0.,.85));
 color=vec4(c,opacity);
 #endif
}`;
}
const skyVert=`out vec2 clipCoord;void main(){clipCoord=vec2((gl_VertexID==1)?3.:-1.,(gl_VertexID==2)?3.:-1.);gl_Position=vec4(clipCoord,.999999,1.);}`;
const skyFrag=projection+`in vec2 clipCoord;uniform float daylight;uniform vec3 disk0,disk1;uniform vec2 sunPower;uniform bool immersed;out vec4 color;
void main(){
 vec3 ray=normalize(forward+rightDir*clipCoord.x*viewport.x/(2.*focal)+upDir*clipCoord.y*viewport.y/(2.*focal));
 vec3 c=mix(vec3(.14,.21,.22),vec3(.39,.54,.52),clamp(.5+.55*ray.z,0.,1.))*(.48+.52*daylight);
 c+=vec3(1.,.85,.55)*(1.-smoothstep(.015,.023,length(ray-disk0)))*sunPower.x;
 c+=vec3(1.,.68,.36)*(1.-smoothstep(.012,.019,length(ray-disk1)))*sunPower.y;
 if(immersed)c=vec3(.10,.28,.29);color=vec4(c,1.);
}`;

function icosphere(){
  const g=(1+Math.sqrt(5))/2;
  const p=[[-1,g,0],[1,g,0],[-1,-g,0],[1,-g,0],[0,-1,g],[0,1,g],[0,-1,-g],[0,1,-g],[g,0,-1],[g,0,1],[-g,0,-1],[-g,0,1]];
  const faces=[[0,11,5],[0,5,1],[0,1,7],[0,7,10],[0,10,11],[1,5,9],[5,11,4],[11,10,2],[10,7,6],[7,1,8],[3,9,4],[3,4,2],[3,2,6],[3,6,8],[3,8,9],[4,9,5],[2,4,11],[6,2,10],[8,6,7],[9,8,1]];
  const cache=new Map(),tris=[];
  function mid(a,b){const key=Math.min(a,b)*100+Math.max(a,b);if(!cache.has(key)){cache.set(key,p.length);p.push(p[a].map((v,k)=>(v+p[b][k])/2));}return cache.get(key);}
  for(const [a,b,c] of faces){const ab=mid(a,b),bc=mid(b,c),ca=mid(c,a);tris.push(a,ab,ca,b,bc,ab,c,ca,bc,ab,bc,ca);}
  return {vertices:new Float32Array(p.flatMap(v=>v.map(x=>x/Math.hypot(...v)))),indices:new Uint16Array(tris)};
}

// The packed far pass covers every live UID. Only the close-up meshes need gathered instances.
function makeRenderer() {
  const g=gl, locs=new Map();
  const make=(v,f)=>program(g,v,f);
  const programs={body:make(projection+bodyVert,bodyFrag(false,false)),pick:make(projection+bodyVert,bodyFrag(false,true)),
    mesh:make("#define NEAR_MESH\n"+projection+bodyVert,bodyFrag(true,false)),
    pickMesh:make("#define NEAR_MESH\n"+projection+bodyVert,bodyFrag(true,true)),
    land:make(landVert,landFragment(false)),landPick:make(landVert,landFragment(true)),sky:make(skyVert,skyFrag)};
  function uniform(p,key) {
    if(!locs.has(p))locs.set(p,new Map());
    const m=locs.get(p);
    if(!m.has(key))m.set(key,g.getUniformLocation(p,key));
    return m.get(key);
  }
  const data=(buffer,array)=>{g.bindBuffer(g.ARRAY_BUFFER,buffer);g.bufferData(g.ARRAY_BUFFER,array,g.STREAM_DRAW);};
  function attribute(b,i,n,type,stride,offset,divisor=0,integer=true){
    g.bindBuffer(g.ARRAY_BUFFER,b);g.enableVertexAttribArray(i);
    if(integer)g.vertexAttribIPointer(i,n,type,stride,offset);else g.vertexAttribPointer(i,n,type,false,stride,offset);
    g.vertexAttribDivisor(i,divisor);
  }
  function bodyArrays(near){
    const vao=g.createVertexArray(),record=g.createBuffer(),prev=g.createBuffer();
    const d=near?1:0;
    g.bindVertexArray(vao);
    attribute(record,0,3,g.UNSIGNED_SHORT,24,8,d);
    attribute(record,1,2,g.UNSIGNED_BYTE,24,14,d);
    attribute(record,2,4,g.UNSIGNED_BYTE,24,16,d);
    attribute(record,7,2,g.UNSIGNED_INT,24,0,d);
    attribute(prev,3,4,g.UNSIGNED_SHORT,8,0,d);
    const ids=g.createBuffer(),shape=g.createBuffer(),mask=g.createBuffer();let numIndices=0;
    if(!near)attribute(mask,8,1,g.UNSIGNED_BYTE,0,0);
    if(near){
      const mesh=icosphere(), verts=g.createBuffer(),idx=g.createBuffer();data(verts,mesh.vertices);
      attribute(verts,4,3,g.FLOAT,0,0,0,false);attribute(ids,5,1,g.UNSIGNED_INT,0,0,1);
      attribute(shape,6,4,g.FLOAT,0,0,1,false);
      g.bindBuffer(g.ELEMENT_ARRAY_BUFFER,idx);g.bufferData(g.ELEMENT_ARRAY_BUFFER,mesh.indices,g.STATIC_DRAW);numIndices=mesh.indices.length;
    }
    g.bindVertexArray(null);return {vao,record,prev,ids,shape,mask,numIndices,count:0};
  }
  const far=bodyArrays(false),near=bodyArrays(true);
  let closeVAO=null,closeRecord,closePrevious,closeIndices,closeShapes,materialBuffer;
  function prepareClose(){
    if(closeVAO)return;
    const shaded=make(projection+closeVert,closeFrag(false));
    const picked=make(projection+closeVert,closeFrag(true));
    programs.close=shaded;programs.pickClose=picked;
    closeVAO=g.createVertexArray();
    closeRecord=g.createBuffer();closePrevious=g.createBuffer();closeIndices=g.createBuffer();
    closeShapes=g.createBuffer();materialBuffer=g.createBuffer();
    g.bindVertexArray(closeVAO);
    attribute(closeRecord,0,3,g.UNSIGNED_SHORT,24,8,1);
    attribute(closeRecord,1,2,g.UNSIGNED_BYTE,24,14,1);
    attribute(closeRecord,2,4,g.UNSIGNED_BYTE,24,16,1);
    attribute(closeRecord,7,2,g.UNSIGNED_INT,24,0,1);
    attribute(closePrevious,3,4,g.UNSIGNED_SHORT,8,0,1);
    attribute(closeIndices,5,1,g.UNSIGNED_INT,0,0,1);
    attribute(closeShapes,6,4,g.FLOAT,0,0,1,false);
    attribute(materialBuffer,9,4,g.UNSIGNED_BYTE,5,0,1);
    attribute(materialBuffer,10,1,g.UNSIGNED_BYTE,5,4,1);
    g.bindVertexArray(null);
  }
  g.vertexAttribI4ui(1,0,0,0,0);
  const plain=g.createVertexArray(),vox=g.createVertexArray(),vents=g.createVertexArray(),box=g.createVertexArray();
  const vi=g.createBuffer(),ei=g.createBuffer(),colors=g.createBuffer(),lines=g.createBuffer(),cube=g.createBuffer();
  g.bindVertexArray(vox);attribute(vi,1,1,g.UNSIGNED_INT,4,0,1);
  g.bindBuffer(g.ELEMENT_ARRAY_BUFFER,cube);
  g.bufferData(g.ELEMENT_ARRAY_BUFFER,new Uint16Array([0,2,1,1,2,3,4,5,6,5,7,6,0,1,4,1,5,4,2,6,3,3,6,7,0,4,2,2,4,6,1,3,5,3,7,5]),g.STATIC_DRAW);
  g.bindVertexArray(vents);attribute(ei,1,1,g.UNSIGNED_INT,0,0);attribute(colors,0,3,g.FLOAT,0,0,0,false);
  g.bindVertexArray(box);attribute(lines,0,3,g.FLOAT,0,0,0,false);g.bindVertexArray(null);
  const tex=(target)=>{const t=g.createTexture();g.bindTexture(target,t);for(const x of [g.TEXTURE_MIN_FILTER,g.TEXTURE_MAG_FILTER])g.texParameteri(target,x,g.NEAREST);
    for(const wrap of [g.TEXTURE_WRAP_S,g.TEXTURE_WRAP_T])g.texParameteri(target,wrap,g.CLAMP_TO_EDGE);return t;};
  const ground=tex(g.TEXTURE_2D),water=tex(g.TEXTURE_2D),color=tex(g.TEXTURE_2D),atlas=tex(g.TEXTURE_2D);
  const atlasWidth=Math.min(4096,g.getParameter(g.MAX_TEXTURE_SIZE)), tilesPerRow=Math.floor(atlasWidth/48),layers=tilesPerRow*tilesPerRow;
  g.texStorage2D(g.TEXTURE_2D,1,g.RG8,atlasWidth,atlasWidth);
  g.texParameteri(g.TEXTURE_2D,g.TEXTURE_MIN_FILTER,g.LINEAR);g.texParameteri(g.TEXTURE_2D,g.TEXTURE_MAG_FILTER,g.LINEAR);
  const layerCache=new Map();let solids=0,markers=0,daylight=1,pickTarget=null,curW=0,curH=0;
  function tex2(t,fmt,channel,type,w,h,pixels){
    g.bindTexture(g.TEXTURE_2D,t);g.pixelStorei(g.UNPACK_ALIGNMENT,1);g.texImage2D(g.TEXTURE_2D,0,fmt,w,h,0,channel,type,pixels);
  }
  function terrain(t){
    const [w,h,z]=t.dims;
    tex2(ground,g.R32UI,g.RED_INTEGER,g.UNSIGNED_INT,w,h,t.heights);
    tex2(water,g.R32UI,g.RED_INTEGER,g.UNSIGNED_INT,w,h,new Uint32Array(w*h));
    const solid=[];
    for(let i=0;i<t.rock.length;i++)if(t.rock[i]===255)solid.push(i);
    data(vi,Uint32Array.from(solid));solids=solid.length;
    data(ei,Uint32Array.from(t.vents));markers=t.vents.length;
    const edges=[];
    for(let i=0;i<8;i++)for(let k=0;k<3;k++)if(!(i&(1<<k)))for(const a of [i,i+(1<<k)])edges.push(a&1?w:0,a&2?h:0,a&4?z:0);
    data(lines,new Float32Array(edges));fields(t,state.fields);
  }
  function fields(t,f){
    if(!t)return;
    const [w,h,z]=t.dims,n=w*h,layer=Math.min(z-1,Number($("layer").value));
    const bytes=new Uint8Array(4*n), values=f&&sameDims(t.dims,f.dims)?f.fields:null;
    if(f?.water && sameDims(t.dims,f.dims))tex2(water,g.R32UI,g.RED_INTEGER,g.UNSIGNED_INT,w,h,f.water);
    let light=0;
    for(let xy=0;xy<n;xy++){
      const topRock=Math.min(z-1,Math.floor(t.heights[xy]/256)),j=(layer*n+xy)*4;
      bytes[4*xy]=values?values[4*(topRock*n+xy)+3]:Math.min(255,Math.round(255*topRock/z));
      bytes[4*xy+1]=values?(values[j]&3)*85:0;bytes[4*xy+2]=values?values[j+1]:0;bytes[4*xy+3]=values?values[j+2]:0;
      if(values)light+=values[4*((z-1)*n+xy)+1];
    }
    daylight=values?light/(255*n):1;
    tex2(color,g.RGBA8,g.RGBA,g.UNSIGNED_BYTE,w,h,bytes);
    const vc=new Float32Array(t.vents.length*3);
    for(let i=0;i<t.vents.length;i++){const temp=values?values[t.vents[i]*4+2]/255:.5;vc.set([.85+.15*temp,.8-.45*temp,.16],i*3);}
    data(colors,vc);
  }
  let detailMask=new Uint8Array(0),lastNearIDs=new Uint32Array(0);
  function upload(s,prev){
    if(detailMask.length!==s.count)detailMask=new Uint8Array(s.count);
    else for(const id of lastNearIDs)detailMask[id]=0;
    lastNearIDs=new Uint32Array(0);data(far.mask,detailMask);
    data(far.record,s.bytes);data(far.prev,prev);far.count=s.count;near.count=0;state.closeCount=0;
  }
  function uploadNear(m){
    const ids=new Uint32Array(m.ids),words=new Uint32Array(m.packed),v=new Float32Array(4*ids.length),needed=new Map();
    g.activeTexture(g.TEXTURE3);g.bindTexture(g.TEXTURE_2D,atlas);
    for(let i=0;i<ids.length;i++){
      const id=words[i*6+5],s=state.shapes.get(id);
      if(needed.has(id)||!s||needed.size>=layers)continue;
      needed.set(id,s);
    }
    // Keep this draw's resident layers stable, evicting only unused clades.
    const used=new Set([...needed.keys()].map(id=>layerCache.get(id)?.layer).filter(x=>x!==undefined));
    let slot=0,missing=0;
    for(const [id,s] of needed){
      let item=layerCache.get(id);
      if(!item){while(used.has(slot))slot++;const l=slot++;used.add(l);
        for(const [old,e] of layerCache)if(e.layer===l){layerCache.delete(old);break;}
        item={layer:l,source:null};layerCache.set(id,item);
      }
      if(item.source!==s){
        const tile=new Uint8Array(4608);
        for(let k=0;k<2304;k++){tile[k*2]=s.base[k];tile[k*2+1]=s.spikes[k];}
        g.texSubImage2D(g.TEXTURE_2D,0,(item.layer%tilesPerRow)*48,Math.floor(item.layer/tilesPerRow)*48,48,48,g.RG,g.UNSIGNED_BYTE,tile);item.source=s;
      }
    }
    for(let i=0;i<ids.length;i++){
      const id=words[i*6+5],s=needed.get(id);
      if(s)v.set([layerCache.get(id).layer,s.elong,s.spike],i*4);
      else {v[i*4]=-1;missing++;}
    }
    state.closeCount=0;
    if($("detail").value==="close" && m.detailIDs.byteLength){
      prepareClose();
      const indices=new Uint32Array(m.detailIDs),count=indices.length,packed=new Uint8Array(m.packed),prev=new Uint16Array(m.prev);
      const records=new Uint8Array(24*count),previous=new Uint16Array(4*count),original=new Uint32Array(count);
      const shapes=new Float32Array(4*count),material=new Uint8Array(5*count);
      for(const i of indices){
        const s=needed.get(words[i*6+5]);
        if(!s)continue;
        v[i*4+3]=s.gmax;
        const body=s.body,tagHue=packed[i*24+17],tagPattern=packed[i*24+18];
        const hue=(body[8]+Math.trunc((tagHue-128)*3/8))&255;
        const pattern=Math.max(0,Math.min(255,body[10]+tagPattern-128));
        const j=state.closeCount++;
        records.set(packed.subarray(24*i,24*i+24),24*j);
        previous.set(prev.subarray(4*i,4*i+4),4*j);
        shapes.set(v.subarray(4*i,4*i+4),4*j);
        original[j]=ids[i];
        material.set([hue,body[9],pattern,body[11],body[12]],5*j);
      }
      data(closeRecord,records);data(closePrevious,previous);data(closeShapes,shapes);
      data(closeIndices,original);data(materialBuffer,material);
    }
    for(const id of lastNearIDs)detailMask[id]=0;
    for(const id of ids)detailMask[id]=1;
    if(lastNearIDs.length || ids.length)data(far.mask,detailMask);
    lastNearIDs=ids;state.shapeMissing=missing;
    data(near.record,new Uint8Array(m.packed));data(near.prev,new Uint16Array(m.prev));data(near.ids,ids);data(near.shape,v);near.count=ids.length;
  }
  function bind(p,c,now){
    g.useProgram(p);
    const dims=sceneCamera(),s=state.snapshot;
    const light=s?.light||[.1,.3,.8],len=Math.hypot(...light)||1;
    for(const [k,v] of Object.entries({eye:c.eye,forward:c.forward,rightDir:c.right,upDir:c.up,dims,sun:light.map(x=>x/len)}))g.uniform3fv(uniform(p,k),v);
    g.uniform2f(uniform(p,"viewport"),canvas.width,canvas.height);g.uniform1f(uniform(p,"focal"),c.focal);
    const age=Math.max(0,now-latestArrival);
    g.uniform1f(uniform(p,"alpha"),$("smooth").value==="off"?1:Math.min(1,age/snapInterval));g.uniform1f(uniform(p,"birthFade"),Math.min(1,age/200));
    const [x,y,z]=c.eye, [w,h]=dims, ix=(Math.floor(x)%w+w)%w,iy=(Math.floor(y)%h+h)%h;
    const below=state.fields?.water && z>=0 && z<state.fields.water[ix+w*iy]/256;
    g.uniform1i(uniform(p,"immersed"),below?1:0);g.uniform1f(uniform(p,"daylight"),daylight);
    if(p===programs.sky){
      const phases=latestMeta?.sun_phase??[0,0], intensities=latestMeta?.sun_light??[0,0];
      const a=phases[0]*2*Math.PI/65536,b=phases[1]*2*Math.PI/65536;
      g.uniform3f(uniform(p,"disk0"),Math.cos(a),0,Math.sin(a));g.uniform3f(uniform(p,"disk1"),0,Math.cos(b),Math.sin(b));
      g.uniform2f(uniform(p,"sunPower"),Math.min(1,intensities[0]/255),Math.min(1,intensities[1]/255));
    }
    g.uniform1i(uniform(p,"hasChosen"),state.selected?1:0);
    g.uniform2ui(uniform(p,"chosen"),state.selected?parseInt(state.selected.slice(8),16):0,state.selected?parseInt(state.selected.slice(0,8),16):0);
    g.uniform1i(uniform(p,"tilesPerRow"),tilesPerRow);g.uniform1i(uniform(p,"haveWater"),state.fields?.water?1:0);g.uniform1i(uniform(p,"surfaceWater"),1);
    g.uniform1i(uniform(p,"gridOn"),$("grid").checked?1:0);
    g.uniform1i(uniform(p,"sliceMode"),Number($("slice").value));g.uniform1i(uniform(p,"sliceZ"),Number($("layer").value));
  }
  function render(pick=false){
    const c=camera(),now=performance.now(),t=state.terrain;
    g.viewport(0,0,canvas.width,canvas.height);g.enable(g.DEPTH_TEST);g.depthMask(true);g.disable(g.BLEND);
    if(pick)g.clearBufferuiv(g.COLOR,0,new Uint32Array(4));
    else {g.clearColor(.14,.22,.22,1);g.clear(g.COLOR_BUFFER_BIT);}
    g.clear(g.DEPTH_BUFFER_BIT);
    if(!pick){g.disable(g.DEPTH_TEST);bind(programs.sky,c,now);g.bindVertexArray(plain);g.drawArrays(g.TRIANGLES,0,3);g.enable(g.DEPTH_TEST);}
    const p=pick?programs.landPick:programs.land;
    function selectLand(kind){bind(p,c,now);g.uniform1i(uniform(p,"layerKind"),kind);
      for(const [i,name,tex] of [[0,"groundTex",ground],[1,"waterTex",water],[2,"terrainColor",color]]){
        g.activeTexture(g.TEXTURE0+i);g.bindTexture(g.TEXTURE_2D,tex);g.uniform1i(uniform(p,name),i);
      }
    }
    if(t){
      if($("surface").value==="voxels"){selectLand(3);g.bindVertexArray(vox);g.drawElementsInstanced(g.TRIANGLES,36,g.UNSIGNED_SHORT,0,solids);}
      else{selectLand(0);g.bindVertexArray(plain);g.drawArraysInstanced(g.TRIANGLES,0,6,t.heights.length);}
      if(!pick){selectLand(4);g.bindVertexArray(vents);g.drawArrays(g.POINTS,0,markers);
        if($("grid").checked){selectLand(5);g.bindVertexArray(box);g.drawArrays(g.LINES,0,24);}
      }
    }
    if(far.count){
      g.activeTexture(g.TEXTURE1);g.bindTexture(g.TEXTURE_2D,water);
      const fp=pick?programs.pick:programs.body;bind(fp,c,now);
      g.activeTexture(g.TEXTURE3);g.bindTexture(g.TEXTURE_2D,atlas);g.uniform1i(uniform(fp,"atlas"),3);g.bindVertexArray(far.vao);
      g.drawArrays(g.POINTS,0,far.count);
      if(near.count){const mp=pick?programs.pickMesh:programs.mesh;bind(mp,c,now);g.uniform1i(uniform(mp,"atlas"),3);
        g.bindVertexArray(near.vao);g.drawElementsInstanced(g.TRIANGLES,near.numIndices,g.UNSIGNED_SHORT,0,near.count);}
      if(state.closeCount){
        const cp=pick?programs.pickClose:programs.close;
        bind(cp,c,now);g.uniform1i(uniform(cp,"atlas"),3);g.bindVertexArray(closeVAO);
        g.drawArraysInstanced(g.TRIANGLE_STRIP,0,4,state.closeCount);
      }
    }
    if(t && !pick){g.enable(g.BLEND);g.blendFunc(g.SRC_ALPHA,g.ONE_MINUS_SRC_ALPHA);g.depthMask(false);g.bindVertexArray(plain);
      if(state.fields?.water){selectLand(1);g.drawArraysInstanced(g.TRIANGLES,0,6,t.heights.length);}
      if(state.fields?.fields && Number($("slice").value)>=0){selectLand(2);g.drawArraysInstanced(g.TRIANGLES,0,6,t.heights.length);}
      g.depthMask(true);g.disable(g.BLEND);
    }
    g.bindVertexArray(null);return c;
  }
  function pick(x,y){
    if(!pickTarget || curW!==canvas.width || curH!==canvas.height){
      if(pickTarget){g.deleteFramebuffer(pickTarget.fb);g.deleteTexture(pickTarget.tex);g.deleteRenderbuffer(pickTarget.rb);}
      curW=canvas.width;curH=canvas.height;
      const fb=g.createFramebuffer(),ptex=tex(g.TEXTURE_2D),rb=g.createRenderbuffer();
      g.texImage2D(g.TEXTURE_2D,0,g.R32UI,curW,curH,0,g.RED_INTEGER,g.UNSIGNED_INT,null);
      g.bindFramebuffer(g.FRAMEBUFFER,fb);g.framebufferTexture2D(g.FRAMEBUFFER,g.COLOR_ATTACHMENT0,g.TEXTURE_2D,ptex,0);
      g.bindRenderbuffer(g.RENDERBUFFER,rb);g.renderbufferStorage(g.RENDERBUFFER,g.DEPTH_COMPONENT24,curW,curH);g.framebufferRenderbuffer(g.FRAMEBUFFER,g.DEPTH_ATTACHMENT,g.RENDERBUFFER,rb);
      if(g.checkFramebufferStatus(g.FRAMEBUFFER)!==g.FRAMEBUFFER_COMPLETE){g.bindFramebuffer(g.FRAMEBUFFER,null);throw Error("Creature picking framebuffer unavailable");}
      pickTarget={fb,tex:ptex,rb};
    }
    const pv=new Uint32Array(1);g.bindFramebuffer(g.FRAMEBUFFER,pickTarget.fb);
    try{render(true);g.readPixels(x,y,1,1,g.RED_INTEGER,g.UNSIGNED_INT,pv);}finally{g.bindFramebuffer(g.FRAMEBUFFER,null);}
    return pv[0]-1;
  }
  return {render,pick,terrain,fields,upload,uploadNear};
}

let receipt=new Map(),lastSnapSeq=null,hiddenAt=0,proofRequest=null;
function trimMap(map,limit=96){while(map.size>limit)map.delete(map.keys().next().value);}
function updateAge(tick) {
  const arrival=receipt.get(tick), ended=ageByTick.get(tick);
  if(arrival===undefined || ended===undefined)return;
  const age=arrival-ended;
  if(age>=0){ageAtArrival=age;if(measure)measure.ages.push(age);}
  ageByTick.delete(tick);receipt.delete(tick);
}
function initGL(){
  gl=canvas.getContext("webgl2",{alpha:false,antialias:false,depth:true,powerPreference:"high-performance"});
  if(!gl)throw Error("WebGL2 is unavailable in this browser.");
  renderer=makeRenderer();const info=gl.getExtension("WEBGL_debug_renderer_info");
  state.gpu=info?gl.getParameter(info.UNMASKED_RENDERER_WEBGL):gl.getParameter(gl.RENDERER);
  setText("gpu",state.gpu+" | AZST v"+VERSION);
  if(state.terrain)renderer.terrain(state.terrain);
  if(state.snapshot)renderer.upload(state.snapshot,state.previous);
  if(state.near && state.snapshot?.seq===state.near.seq)renderer.uploadNear(state.near);
}
function inspectReply(j){
  if(j.uid!==state.selected)return;
  const list=$("inspect");list.replaceChildren();
  if(!j.found){setText("selected",j.uid+" was absent at tick "+j.tick+".");$("follow").disabled=true;return;}
  setText("selected","UID "+j.uid);
  for(const [key,name] of Object.entries({tick:"Observed tick",generation:"Generation",age:"Age",clade:"Clade",lineage:"Lineage",genes:"Genes",neurons:"Neurons",tiles:"Brain tiles",parts:"Body parts",units:"Units",mass:"Mass",energy:"Energy",health:"Health",colony:"Colony",parent_a:"Parent A",parent_b:"Parent B",pos:"Position, world fractions"})){
    if(!(key in j))continue;
    const dt=document.createElement("dt"),dd=document.createElement("dd");dt.textContent=name;dd.textContent=String(j[key]);list.append(dt,dd);
  }
  $("follow").disabled=false;
}
function applyJSON(j){
  if(j.event==="start"){
    setText("worldMode",j.mode==="wild"?"Wild world":"Lab world");$("rate").value=j.sps??15;$("speed").value=j.tps??0;
    if(j.cap && j.grid){
      const lockTxt=j.locked?"locked in RAM":"pageable";
      setText("popStatus","Active: "+Number(j.cap).toLocaleString()+" slots | grid "+j.grid.join("x")+" | "+j.workers+" workers | "+lockTxt);
    }
  } else if(j.event==="stats"){
    latestMeta=j;paused=!!j.paused;$("pause").textContent=paused?"Run":"Pause";
    if(Number.isFinite(j.tick_end_unix_ms)){const tick=String(j.tick);ageByTick.set(tick,j.tick_end_unix_ms);trimMap(ageByTick);updateAge(tick);}
    if(measure)measure.stats.push({tick:j.tick,live:j.live,ms_copy:j.ms_copy,ms_tick:j.ms_tick,failed:j.observer_fail,tps:j.ticks_per_s});
    if(j.top)setText("legend","Clade populations: "+JSON.stringify(j.top));
  } else if(j.event==="end")endWorld("Engine ended at tick "+j.tick);
  else if(j.event==="error")issue([j.what,j.detail].filter(Boolean).join(" - "));
}
function endWorld(status){
  engineEnded=true;
  if(measure)measure.ended=true;
  $("measure").disabled=true;$("proof").disabled=true;$("save").disabled=true;
  setText("connection",status);
}
function message(evt){
  const m=evt.data;
  if(m.kind==="restart"){location.reload();return;}
  if(m.kind==="ended"){
    if(m.error)issue(m.error);
    if(!engineEnded)endWorld(m.error?"Engine stopped with an error":"Engine ended");
    return;
  }
  if(m.kind==="status"){if(!engineEnded)setText("connection",m.status);return;}
  if(m.kind==="snapshot"){
    try{
      const s=decodeCreatures(decodeFrame(m.buffer)),prev=new Uint16Array(m.prev),t=performance.now();
      if(lastSnapSeq!==null && s.seq!==lastSnapSeq && latestArrival>0)snapInterval=Math.max(1,t-latestArrival);
      latestArrival=t;lastSnapSeq=s.seq;state.snapshot=s;state.previous=prev;state.near=null;
      const tick=String(s.tick);receipt.set(tick,m.arrived);trimMap(receipt);updateAge(tick);
      arrivals.push(m.arrived);if(arrivals.length>240)arrivals.shift();
      if(measure)measure.snapshots.push({arrival_ms:m.arrived,tick,count:s.count,seq:s.seq,before:hex(s.before),after:hex(s.after),failures:s.failures});
      if(!lost && renderer)renderer.upload(s,prev);
      $("empty").hidden=!!renderer && !lost;$("measure").disabled=engineEnded || !renderer || !!measure;$("proof").disabled=engineEnded || !renderer || !!proofRequest;
      if(state.selected && findUid(s,state.selected)<0){$("follow").disabled=true;if(view.mode==="follow")setMode("orbit");}
    }finally{worker.postMessage({kind:"ready"});}
    return;
  }
  if(m.kind==="legend"){setText("legend","Largest clades at tick "+m.tick+": "+m.top.map(([id,n])=>"#"+id+" ("+n.toLocaleString()+")").join(", "));return;}
  if(m.kind==="near"){if(m.seq===state.snapshot?.seq){state.near=m;if(renderer && !lost)renderer.uploadNear(m);}return;}
  if(m.kind!=="frame")return;
  const f=decodeFrame(m.buffer);
  if(f.type===TYPE.JSON)applyJSON(decodeJSON(f));
  if(f.type===TYPE.STATIC){
    state.terrain=decodeStatic(f);$("layer").max=state.terrain.dims[2]-1;
    if(initialCloseCamera){
      view.distance=.8*Math.min(...state.terrain.dims);
      initialCloseCamera=false;
    }
    if(renderer && !lost)renderer.terrain(state.terrain);
  }else if(f.type===TYPE.FIELDS){
    state.fields=decodeFields(f);if(renderer && !lost)renderer.fields(state.terrain,state.fields);
  }else if(f.type===TYPE.SHAPES){let changed=false;for(const s of decodeShapes(f)){
      const old=state.shapes.get(s.clade);
      if(old && old.body.every((v,k)=>v===s.body[k]))continue;
      state.shapes.set(s.clade,{...s,body:s.body.slice(),base:s.base.slice(),spikes:s.spikes.slice()});changed=true;
    }
    if(changed && state.near && renderer && !lost)renderer.uploadNear(state.near);
  }
  // The async preview is shared by every client. The requested comparison uses
  // only the PNG and engine metadata returned together by POST /api/proof.
}
async function requestPicture(size,cameraCommand,s){
  const res=await fetch("/api/proof",{method:"POST",headers:{"Content-Type":"application/json"},
    body:JSON.stringify({size,camera:cameraCommand})});
  if(!res.ok){const err=await res.json();throw Error(err.error||("HTTP "+res.status));}
  const meta=JSON.parse(res.headers.get("X-AZ-Proof"));
  if(!meta || !Number.isInteger(meta.sequence))throw Error("Proof response lacks its native sequence");
  const png=await res.blob();
  if(state.proofURL)URL.revokeObjectURL(state.proofURL);
  state.proofURL=URL.createObjectURL(png);
  $("proofImage").src=state.proofURL;$("proofImage").hidden=false;
  const matched=String(meta.tick)===String(s.tick),same=meta.hash===meta.hash_after_view;
  const agreed=s.before!==0n && meta.hash===hex(s.before) && s.before===s.after;
  const comparison=matched && agreed?"Matched viewer snapshot":s.before===0n?"Snapshot hash unavailable":"DIFFERENT viewer snapshot";
  setText("proofStatus","Tick "+meta.tick+" | "+(same?"Engine hashes equal":"DIFFERENT engine hashes")+" | "+comparison+" | "+meta.hash+" | PNG "+meta.sequence);
}
const waitFor=async(test,ms)=>{const until=performance.now()+ms;while(!test()){if(performance.now()>until)throw Error("Timed out waiting for engine");await new Promise(r=>setTimeout(r,50));}};
function cameraKey(c){return JSON.stringify([c,$("detail").value]);}
async function proof(){
  if(engineEnded || proofRequest || !renderer || !state.snapshot)return;
  const resume=!paused;proofRequest={snapshot:null};$("proof").disabled=true;
  $("proofImage").hidden=true;$("proofView").hidden=true;
  try{
    setText("proofStatus","Matching the camera and snapshot...");
    const preSeq=state.snapshot.seq;
    await command("pause");
    await waitFor(()=>latestMeta?.paused && latestMeta.tick===Number(state.snapshot.tick) && (state.snapshot.seq!==preSeq || !resume),8000);
    if($("detail").value==="close"){
      await waitFor(()=>state.near?.seq===state.snapshot.seq && state.near.cameraKey===cameraKey(camera()),8000);
    }
    const c=camera(),target=c.eye.map((v,i)=>v+c.forward[i]*c.distance);
    const aspect=canvas.width/canvas.height, sw=Math.max(160,Math.min(1920,Math.round(900*aspect))), sh=Math.max(90,Math.min(1080,Math.round(sw/aspect)));
    const size="size "+sw+"x"+sh;
    const cameraCommand="camera "+[view.yaw,view.pitch,c.distance,...target].map(x=>x.toFixed(6)).join(" ");
    proofRequest.snapshot=state.snapshot;
    const smooth=$("smooth").value;$("smooth").value="off";
    try{renderer.render();$("proofView").src=canvas.toDataURL("image/png");$("proofView").hidden=false;}finally{$("smooth").value=smooth;}
    await requestPicture(size,cameraCommand,proofRequest.snapshot);
  }catch(e){setText("proofStatus",e.message);}
  finally{proofRequest=null;$("proof").disabled=engineEnded;if(resume && !engineEnded)await command("run");}
}
function setMode(mode){
  if(mode==="follow" && (!state.selected || findUid(state.snapshot,state.selected)<0))return;
  if(mode==="fly" && view.mode!=="fly")view.eye=camera().eye.slice();
  view.mode=mode;for(const b of document.querySelectorAll("[data-mode]"))b.setAttribute("aria-pressed",String(b.dataset.mode===mode));
  setText("cameraHint",mode==="fly"?"Drag to look / WASD to fly / Q and E for height / Wheel for speed":"Drag to orbit / Scroll to zoom / Click a creature");
}
async function selectAt(e){
  if(!renderer || !state.snapshot)return;
  const r=canvas.getBoundingClientRect(),x=Math.floor((e.clientX-r.left)*canvas.width/r.width),y=canvas.height-1-Math.floor((e.clientY-r.top)*canvas.height/r.height);
  const i=renderer.pick(x,y);
  if(i<0 || i>=state.snapshot.count){setText("selected","No visible creature at this point.");return;}
  state.selected=uidAt(state.snapshot,i);setText("selected","Reading UID "+state.selected);$("follow").disabled=false;
  inspectReply(await post("/api/inspect",{uid:state.selected}));
}
function startMeasurement(){
  if(engineEnded||!state.snapshot||measure)return;
  measure={started:performance.now(),started_utc:new Date().toISOString(),samples:[],snapshots:[],stats:[],ages:[],hidden_ms:0,gpu:state.gpu,
    viewport:[canvas.width,canvas.height],world_mode:state.snapshot.wild?"wild":"lab",start_tick:String(state.snapshot.tick),start_count:state.snapshot.count};
  $("measure").disabled=true;setText("measureStatus","Measuring for 30 seconds...");
}
async function saveMeasurement(t){
  const m=measure;measure=null;$("measure").disabled=engineEnded;
  const duration=(t-m.started)/1000,p50=quantile(m.samples,.5),p99=quantile(m.samples,.99);
  if(!p50){setText("measureStatus","Too few frames were presented.");return;}
  const record={format:"AZST viewer performance v1",gpu:m.gpu,viewport:m.viewport,mode:m.world_mode,start_tick:m.start_tick,end_tick:String(state.snapshot.tick),
    start_count:m.start_count,end_count:state.snapshot.count,started_utc:m.started_utc,duration_s:duration,
    median_fps:1000/p50,frame_p99_ms:p99,frame_count:m.samples.length,snapshots_per_s:m.snapshots.length/duration,
    ticks_per_s:Number(state.snapshot.tick-BigInt(m.start_tick))/duration,snapshot_age_median_ms:quantile(m.ages,.5),hidden_ms:m.hidden_ms,
    engine_ended_during_measurement:!!m.ended,observer_failures:state.snapshot.failures,dt_ms:m.samples,snapshots:m.snapshots,engine_samples:m.stats};
  try{const res=await post("/api/perf",record);
    setText("measureStatus",record.median_fps.toFixed(1)+" FPS median / "+p99.toFixed(1)+" ms p99 / "+record.snapshots_per_s.toFixed(1)+" snapshots/s. Saved "+res.file+(m.hidden_ms?" (backgrounded)":"")+(m.ended?" (engine ended during measurement)":"")+(m.snapshots.length?"":" (no new snapshots)"));
  }catch(e){setText("measureStatus",e.message);}
}
function loop(t){
  const dt=lastRAF?t-lastRAF:0;lastRAF=t;
  if(dt>0 && state.snapshot && renderer && !lost){frameTimes.push(dt);if(frameTimes.length>120)frameTimes.shift();if(measure)measure.samples.push(dt);}
  if(renderer && !lost){
    const rect=canvas.getBoundingClientRect(),scale=window.devicePixelRatio||1;
    const w=Math.max(1,Math.round(rect.width*scale)),h=Math.max(1,Math.round(rect.height*scale));
    if(canvas.width!==w || canvas.height!==h){canvas.width=w;canvas.height=h;}
    if(view.mode==="fly" && view.eye){
      const c=camera(),speed=view.speed*Math.min(dt/1000,.1);
      const f=(keys.has("w")?1:0)-(keys.has("s")?1:0),r=(keys.has("d")?1:0)-(keys.has("a")?1:0),u=(keys.has("e")?1:0)-(keys.has("q")?1:0);
      view.eye=view.eye.map((x,k)=>x+speed*(f*c.forward[k]+r*c.right[k]+(k===2?u:0)));
    }
    const c=renderer.render();
    if(t-lastNearSend>100 && worker){const signature=cameraKey(c);if(signature!==lastCamera){worker.postMessage({kind:"camera",camera:{...c,detail:$("detail").value==="close",key:signature}});lastCamera=signature;}lastNearSend=t;}
  }
  if(t-lastUI>250 && state.snapshot){
    const s=state.snapshot;setText("population",s.count.toLocaleString());setText("worldTick","Tick "+s.tick);setText("worldMode",s.wild?"Wild world":"Lab world");
    const p50=quantile(frameTimes,.5),p99=quantile(frameTimes,.99);setText("fps",p50?(1000/p50).toFixed(1):"--");setText("p99",p99?p99.toFixed(1)+" ms":"--");
    const recent=arrivals.filter(x=>x>=nowAbs()-3000);setText("sps",recent.length>1?((recent.length-1)*1000/(recent.at(-1)-recent[0])).toFixed(1)+" / s":"0 / s");
    setText("tps",Number(latestMeta?.ticks_per_s||0).toFixed(1)+" / s");setText("age",ageAtArrival===null?"Unavailable":ageAtArrival.toFixed(0)+" ms");
    setText("lastReceipt",Math.max(0,nowAbs()-arrivals.at(-1)).toFixed(0)+" ms ago");
    setText("observer",s.failures+" / "+(latestMeta?.observer_pass??0)+" passed");setText("shapes",state.shapes.size+" clade shapes / "+state.shapeMissing+" close bodies using spheres / "+state.closeCount+" detailed close-ups");
    if(measure)setText("measureStatus",Math.max(0,Math.ceil(30-(t-measure.started)/1000))+" seconds remaining");lastUI=t;
  }
  if(measure && t-measure.started>=30000)saveMeasurement(t);
  requestAnimationFrame(loop);
}
function action(cb){return (...args)=>{try{Promise.resolve(cb(...args)).catch(e=>issue(e.message));}catch(e){issue(e.message);}};}
let drag=null;
canvas.addEventListener("pointerdown",e=>{canvas.focus();drag={x:e.clientX,y:e.clientY,moved:0};canvas.setPointerCapture(e.pointerId);});
canvas.addEventListener("pointermove",e=>{if(!drag)return;const dx=e.clientX-drag.x,dy=e.clientY-drag.y;drag.moved+=Math.abs(dx)+Math.abs(dy);
  drag.x=e.clientX;drag.y=e.clientY;view.yaw=((view.yaw-dx*.006)%(2*Math.PI)+2*Math.PI)%(2*Math.PI);view.pitch=Math.min(1.55,Math.max(-1.55,view.pitch+dy*.006));});
canvas.addEventListener("pointerup",action(async e=>{const clicked=drag && drag.moved<4;drag=null;if(clicked)await selectAt(e);}));
canvas.addEventListener("pointercancel",()=>{drag=null;});
canvas.addEventListener("wheel",e=>{e.preventDefault();const f=Math.exp(Math.sign(e.deltaY)*.12);
  if(view.mode==="fly")view.speed=Math.min(5000,Math.max(.01,view.speed/f));else view.distance=Math.max(.05,Math.min(65536,(view.distance||camera().distance)*f));},{passive:false});
canvas.addEventListener("keydown",e=>{const k=e.key.toLowerCase();if("wasdqe".includes(k)){keys.add(k);e.preventDefault();}});
canvas.addEventListener("keyup",e=>keys.delete(e.key.toLowerCase()));canvas.addEventListener("blur",()=>keys.clear());
for(const b of document.querySelectorAll("[data-mode]"))b.onclick=()=>setMode(b.dataset.mode);
$("expand").onclick=action(()=>document.fullscreenElement?document.exitFullscreen():$("stage").requestFullscreen());
$("measure").onclick=startMeasurement;$("proof").onclick=action(proof);
$("save").onclick=action(async()=>{
  $("save").disabled=true;setText("saveStatus","Saving this world...");
  try{const result=await post("/api/save",{});setText("saveStatus","Saved "+result.bytes.toLocaleString()+" bytes to "+result.path);}
  catch(e){setText("saveStatus",e.message);}
  finally{$("save").disabled=engineEnded;}
});
$("pause").onclick=action(()=>command(paused?"run":"pause"));$("step").onclick=action(()=>command("step 1"));
$("applyRate").onclick=action(async()=>{if(!$("rate").reportValidity() || !$("speed").reportValidity())return;await command("sps "+$("rate").value);await command("tps "+$("speed").value);});
const POP_MIN=1, POP_MAX=1000000;
let popStep=1000;
function autoCap(n){return n<=250000?Math.min(1004096,n*4+4096):Math.max(n+4096,1004096);}
function autoGrid(n){let b=0;while(b<26 && (2*(2**b))<=n)b++;if(b<5)b=5;const gz=Math.floor(b/3),r=b-gz,gx=Math.floor((r+1)/2),gy=r-gx;return (1<<gx)+"x"+(1<<gy)+"x"+(1<<gz);}
function clampPop(v){const x=Math.round(Number(v));return Number.isFinite(x)?Math.max(POP_MIN,Math.min(POP_MAX,x)):8000;}
function refreshPopUI(){
  const n=clampPop($("popTarget").value);
  $("popTarget").value=String(n);
  const rawCap=Math.max(0,Math.min(2000000,Math.round(Number($("popCap").value)||0)));
  const cap=rawCap?Math.max(n,rawCap):autoCap(n);
  const estMB=cap*0.00885;
  const mem=estMB>=1024?(estMB/1024).toFixed(2)+" GB":estMB.toFixed(0)+" MB";
  setText("popMeta",(rawCap?"Custom":"Auto")+" slots: "+cap.toLocaleString()+" (~"+mem+" RAM | grid "+autoGrid(n)+")");
  setText("popDec","-"+popStep.toLocaleString());
  setText("popInc","+"+popStep.toLocaleString());
}
function setPopStep(step){
  popStep=step;$("popTarget").step=String(step);
  for(const b of document.querySelectorAll("[data-pop-step]"))b.setAttribute("aria-pressed",String(Number(b.dataset.popStep)===step));
  refreshPopUI();
}
function adjustPop(delta){$("popTarget").value=String(clampPop(Number($("popTarget").value||0)+delta));refreshPopUI();}
for(const b of document.querySelectorAll("[data-pop-step]"))b.onclick=()=>setPopStep(Number(b.dataset.popStep));
for(const b of document.querySelectorAll("[data-pop-delta]"))b.onclick=()=>adjustPop(Number(b.dataset.popDelta));
for(const b of document.querySelectorAll("[data-pop-set]"))b.onclick=()=>{$("popTarget").value=String(clampPop(b.dataset.popSet));refreshPopUI();};
$("popDec").onclick=()=>adjustPop(-popStep);
$("popInc").onclick=()=>adjustPop(popStep);
$("popTarget").oninput=refreshPopUI;
$("popCap").oninput=refreshPopUI;
async function applyPopulation(){
  const n=clampPop($("popTarget").value);
  const rawCap=Math.max(0,Math.min(2000000,Math.round(Number($("popCap").value)||0)));
  const cap=rawCap?Math.max(n,rawCap):0;
  const lab=$("popMode").value==="lab";
  const kernels=$("popKernels").checked?1:0;
  $("applyPop").disabled=true;
  setText("popStatus","Launching bare-metal world ("+n.toLocaleString()+" founders)...");
  issue("");
  try{
    await post("/api/world",{n,cap,lab,kernels});
    location.reload();
  }catch(e){
    setText("popStatus",e.message);
    $("applyPop").disabled=false;
  }
}
$("applyPop").onclick=action(applyPopulation);
$("popTarget").addEventListener("keydown",e=>{if(e.key==="Enter"){e.preventDefault();action(applyPopulation)();}});
fetch("/api/state",{cache:"no-store"}).then(r=>r.ok?r.json():null).then(info=>{
  if(info && info.config){
    const n=clampPop(info.config.n??8000);
    $("popTarget").value=String(n);
    $("popCap").value="0";
    $("popMode").value=info.config.lab?"lab":"wild";
    if(info.config.kernels!==undefined)$("popKernels").checked=!!info.config.kernels;
  }
  refreshPopUI();
}).catch(()=>refreshPopUI());
$("detail").onchange=()=>{
  // Drop any old high-detail selection immediately upon returning to overview.
  if(renderer && !lost && state.near)renderer.uploadNear(state.near);
};
$("slice").onchange=()=>{$("sliceRow").hidden=Number($("slice").value)<0;};
$("layer").oninput=()=>{setText("layerNo",$("layer").value);if(renderer && !lost)renderer.fields(state.terrain,state.fields);};
document.addEventListener("visibilitychange",()=>{if(document.hidden)hiddenAt=performance.now();else if(measure && hiddenAt){measure.hidden_ms+=performance.now()-Math.max(hiddenAt,measure.started);hiddenAt=0;}});
canvas.addEventListener("webglcontextlost",e=>{e.preventDefault();lost=true;renderer=null;$("empty").hidden=false;setText("empty","Graphics context lost. Waiting for restoration.");});
canvas.addEventListener("webglcontextrestored",action(()=>{initGL();lost=false;$("empty").hidden=!!state.snapshot;issue("");}));
try{initGL();}catch(e){issue("Graphics: "+e.message);setText("empty","WebGL2 could not start");}
const pollRequested=new URLSearchParams(location.search).get("transport")==="poll";
worker=new Worker(pollRequested?"/viewer_decode.js?transport=poll":"/viewer_decode.js",{type:"module"});
worker.onmessage=action(message);worker.onerror=e=>issue("Stream worker: "+e.message);
requestAnimationFrame(loop);
