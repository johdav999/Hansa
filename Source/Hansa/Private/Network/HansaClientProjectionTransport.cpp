#include "Network/HansaMultiplayerTypes.h"
#include "Misc/Compression.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/NameAsStringProxyArchive.h"

bool FHansaClientProjectionSnapshot::NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
{
 // This property is owner-only. Compress the already-authorized report, never the host state.
 // Names travel as strings: process-local FName indices must not become wire identities.
 constexpr uint32 MaximumRawBytes=8*1024*1024, MaximumWireBytes=60*1024;
 bOutSuccess=false;TArray<uint8> Raw,Compressed;uint32 RawSize=0,WireSize=0;
 if(Ar.IsSaving()){
  FMemoryWriter Writer(Raw,true);FNameAsStringProxyArchive Names(Writer);
  StaticStruct()->SerializeItem(Names,this,nullptr);
  if(Writer.IsError()||Raw.IsEmpty()||Raw.Num()>MaximumRawBytes){Ar.SetError();return false;}
  RawSize=Raw.Num();int32 Bound=FCompression::CompressMemoryBound(NAME_Zlib,Raw.Num());Compressed.SetNumUninitialized(Bound);
  if(!FCompression::CompressMemory(NAME_Zlib,Compressed.GetData(),Bound,Raw.GetData(),Raw.Num())||Bound>MaximumWireBytes){Ar.SetError();return false;}
  WireSize=Bound;Compressed.SetNum(Bound);
 }
 Ar.SerializeIntPacked(RawSize);Ar.SerializeIntPacked(WireSize);
 if(Ar.IsError()||RawSize==0||RawSize>MaximumRawBytes||WireSize==0||WireSize>MaximumWireBytes){if(Ar.IsLoading())*this={};Ar.SetError();return false;}
 if(Ar.IsLoading())Compressed.SetNumUninitialized(WireSize);
 Ar.Serialize(Compressed.GetData(),WireSize);
 if(Ar.IsError()){if(Ar.IsLoading())*this={};return false;}
 if(Ar.IsLoading()){
  Raw.SetNumUninitialized(RawSize);
  if(!FCompression::UncompressMemory(NAME_Zlib,Raw.GetData(),RawSize,Compressed.GetData(),WireSize)){*this={};Ar.SetError();return false;}
  FHansaClientProjectionSnapshot Candidate;FMemoryReader Reader(Raw,true);FNameAsStringProxyArchive Names(Reader);
  StaticStruct()->SerializeItem(Names,&Candidate,nullptr);
  if(Reader.IsError()||Reader.Tell()!=Raw.Num()||Candidate.SchemaVersion!=CurrentSchemaVersion){*this={};Ar.SetError();return false;}
  *this=MoveTemp(Candidate);
 }
 bOutSuccess=true;return true;
}
