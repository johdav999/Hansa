import { WorkerError } from "./errors.js";
import { validateMediaBytes } from "./media-source.js";

function check(ok, message) { if (!ok) throw new WorkerError("StaticPropRejected", message); }
export function staticPropProfile(p) {
  check(p && p.version === 1 && p.role === "HarborProp", "A version 1 HarborProp profile is required.");
  check(Object.keys(p).every(k => ["version","role","stableId","heightCm","maximumTriangles","maximumMaterials","maximumTextureSize","forwardAxis","upAxis","pivot","collision"].includes(k)), "Unknown static prop setting.");
  check(typeof p.stableId === "string" && p.stableId.length <= 100 && /^Prop\.[A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+)*$/.test(p.stableId), "Use a Hansa Prop.* stable ID.");
  for (const [k, min, max] of [["heightCm",10,1000],["maximumTriangles",1,50000],["maximumMaterials",1,8],["maximumTextureSize",64,4096]])
    check(Number.isSafeInteger(p[k]) && p[k] >= min && p[k] <= max, "Invalid bounded static prop setting: " + k);
  check(p.forwardAxis === "+X" && p.upAxis === "+Z" && p.pivot === "bottom-center" && p.collision === "box", "Use +X forward, +Z up, bottom-center pivot and box collision.");
  return { ...p };
}

// Geometry is deliberately a single uncompressed static mesh. FBX, external
// resources, compressed extensions and animation are rejected before native import.
export function validateStaticPropGlb(bytes, profile) {
  const p = staticPropProfile(profile);
  validateMediaBytes(bytes, "model/gltf-binary");
  const length = bytes.readUInt32LE(12);
  const d = JSON.parse(bytes.toString("utf8",20,20+length));
  const bin = bytes.subarray(28+length);
  check(d.meshes.length === 1 && Array.isArray(d.nodes) && d.nodes.length > 0 && d.nodes.length <= 64, "One mesh in a bounded identity scene required.");
  const identity = (value, expected) => value === undefined || Array.isArray(value) && value.length === expected.length && value.every((n,i) => Number.isFinite(n) && Math.abs(n-expected[i]) < 1e-8);
  check(d.scenes?.length === 1 && Array.isArray(d.scenes[0].nodes) && d.scenes[0].nodes.length > 0 && (d.scene ?? 0) === 0, "One deterministic scene required.");
  const visited = new Set(); let instances = 0;
  function visit(index) {
    check(Number.isSafeInteger(index) && index >= 0 && index < d.nodes.length && !visited.has(index), "Invalid, shared or cyclic scene node.");
    visited.add(index); const node = d.nodes[index];
    check(node && identity(node.matrix,[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]) && identity(node.translation,[0,0,0]) && identity(node.rotation,[0,0,0,1]) && identity(node.scale,[1,1,1]), "Bake nonidentity scene transforms before static-prop import.");
    if(node.mesh !== undefined) {check(node.mesh === 0, "Invalid scene mesh.");instances++;}
    check(node.children === undefined || Array.isArray(node.children), "Invalid scene children.");
    for(const child of node.children ?? []) visit(child);
  }
  for(const root of d.scenes[0].nodes) visit(root);
  check(instances === 1 && visited.size === d.nodes.length, "Exactly one mesh instance and no unreachable nodes required.");
  check((d.materials?.length ?? 0) <= p.maximumMaterials, "Material budget exceeded.");
  function view(index) {
    const v = d.bufferViews?.[index];
    check(v && v.buffer === 0 && Number.isSafeInteger(v.byteLength) && v.byteLength > 0 && Number.isSafeInteger(v.byteOffset ?? 0) && (v.byteOffset ?? 0) >= 0 && (v.byteOffset ?? 0)+v.byteLength <= d.buffers[0].byteLength, "Invalid buffer view.");
    return v;
  }
  function accessor(index, type, components, allowed) {
    const a = d.accessors?.[index];
    check(a && !a.sparse && !a.normalized && a.type === type && allowed.includes(a.componentType) && Number.isSafeInteger(a.count) && a.count > 0 && a.count <= 150000, "Invalid static mesh accessor.");
    const v = view(a.bufferView), size = {5121:1,5123:2,5125:4,5126:4}[a.componentType], stride = v.byteStride ?? components*size, off = a.byteOffset ?? 0;
    check(Number.isSafeInteger(off) && off >= 0 && off % size === 0 && Number.isSafeInteger(stride) && stride >= components*size && stride <= 252 && stride % size === 0 && off+(a.count-1)*stride+components*size <= v.byteLength, "Accessor exceeds embedded buffer.");
    return {count:a.count, read(i,c=0) { const pos=(v.byteOffset??0)+off+i*stride+c*size; return a.componentType===5126 ? bin.readFloatLE(pos) : bin.readUIntLE(pos,size); }};
  }
  check(Array.isArray(d.bufferViews) && d.bufferViews.length <= 256 && Array.isArray(d.accessors) && d.accessors.length <= 128, "Excessive static mesh descriptors.");
  for(let index=0;index<d.bufferViews.length;index++) view(index);
  for(let index=0;index<d.accessors.length;index++) {
    const type=d.accessors[index]?.type, components={SCALAR:1,VEC2:2,VEC3:3,VEC4:4}[type];
    check(components,"Unsupported static accessor type.");
    accessor(index,type,components,[5121,5123,5125,5126]);
  }
  let triangles=0;
  const min=[Infinity,Infinity,Infinity], max=[-Infinity,-Infinity,-Infinity];
  check(Array.isArray(d.meshes[0].primitives) && d.meshes[0].primitives.length > 0 && d.meshes[0].primitives.length <= p.maximumMaterials, "Missing mesh primitives.");
  for (const primitive of d.meshes[0].primitives) {
    check((primitive.mode??4)===4 && !primitive.targets && (primitive.material===undefined || Number.isSafeInteger(primitive.material) && primitive.material>=0 && primitive.material<(d.materials?.length??0)), "Only static triangle primitives with valid materials are accepted.");
    const positions=accessor(primitive.attributes?.POSITION,"VEC3",3,[5126]);
    for (let i=0;i<positions.count;i++) for(let c=0;c<3;c++) {const x=positions.read(i,c); check(Number.isFinite(x)&&Math.abs(x)<=100,"Invalid position."); min[c]=Math.min(min[c],x);max[c]=Math.max(max[c],x);}
    for(const [name,index] of Object.entries(primitive.attributes)) {
      if(name==="POSITION") continue;
      check(["NORMAL","TANGENT","TEXCOORD_0","TEXCOORD_1"].includes(name),"Unsupported mesh attribute.");
      const count=name==="TANGENT"?4:name.startsWith("TEXCOORD")?2:3;
      const a=accessor(index,"VEC"+count,count,[5126]); check(a.count===positions.count,"Attribute count mismatch.");
      for(let i=0;i<a.count;i++) for(let c=0;c<count;c++) check(Number.isFinite(a.read(i,c)),"Non-finite mesh attribute.");
    }
    const indices=primitive.indices===undefined?null:accessor(primitive.indices,"SCALAR",1,[5121,5123,5125]);
    const count=indices?.count??positions.count; check(count%3===0,"Incomplete triangle.");
    if(indices) for(let i=0;i<count;i++) check(indices.read(i)<positions.count,"Index exceeds vertex count.");
    triangles+=count/3;
  }
  check(triangles<=p.maximumTriangles,"Triangle budget exceeded.");
  check((d.images?.length??0)<=p.maximumMaterials*4 && (d.textures?.length??0)<=p.maximumMaterials*4,"Texture count exceeded.");
  for(const image of d.images??[]) {
    const v=view(image.bufferView), data=bin.subarray(v.byteOffset??0,(v.byteOffset??0)+v.byteLength);
    const [width,height]=imageDimensions(data,image.mimeType);
    check(width<=p.maximumTextureSize&&height<=p.maximumTextureSize,"Texture dimensions exceed budget.");
  }
  check((d.images?.length??0)<=p.maximumMaterials*4 && (d.textures?.length??0)<=p.maximumMaterials*4,"Texture count exceeded.");
  for(const texture of d.textures??[]) check(Number.isInteger(texture.source)&&texture.source>=0&&texture.source<(d.images?.length??0),"Invalid texture source.");
  return {version:1,triangles,materials:d.materials?.length??0,textures:d.images?.length??0,boundsMeters:{min,max},profile:p};
}
export function imageDimensions(bytes,mime) {
  let width=0,height=0;
  if(mime==="image/png") {
    check(bytes.length>=33&&bytes.subarray(0,8).equals(Buffer.from([137,80,78,71,13,10,26,10]))&&bytes.toString("ascii",12,16)==="IHDR","Invalid PNG header.");
    width=bytes.readUInt32BE(16);height=bytes.readUInt32BE(20);
  } else if(mime==="image/jpeg") {
    check(bytes.length>=4&&bytes[0]===255&&bytes[1]===216,"Invalid JPEG header.");
    let pos=2;
    while(pos+4<=bytes.length) {
      check(bytes[pos]===255,"Invalid JPEG segment."); const marker=bytes[pos+1];pos+=2;
      if(marker===217||marker===218) break;
      const size=bytes.readUInt16BE(pos);check(size>=2&&pos+size<=bytes.length,"Invalid JPEG length.");
      if([192,193,194].includes(marker)) {check(size>=8,"Invalid JPEG frame.");height=bytes.readUInt16BE(pos+3);width=bytes.readUInt16BE(pos+5);break;} pos+=size;
    }
  } else check(false,"Only PNG and JPEG textures are supported.");
  check(width>0&&height>0&&width<=8192&&height<=8192,"Invalid or excessive image dimensions.");
  return [width,height];
}
