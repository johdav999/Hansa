# Laborer Residence R07 Catalog Promotion

Catalog version 17 records the production definition change that points
`Building.Residence.Laborer` at the reviewed R07 mesh:

`/Game/Mesh/hansa-residences/Meshes_R07/SM_Residence_Laborer_A.SM_Residence_Laborer_A`

The focused economic reload diagnostic proved that this is the only definition
fingerprint changed from catalog v16:

- registry: `B65512A7BFAC9E0C` to `968431FAD59A2C51`;
- `Building.Residence.Laborer`: `DFCE9E1B2B778FD6` to `9B8788F64D10F941`;
- all other definition fingerprints are unchanged;
- forward and reverse discovery produce the same registry and fingerprints.

The runtime pin, full golden manifest, seed builder, and reload regression advance
together. The reload test reconstructs the R06 mesh binding and must reproduce the
catalog-v16 hash before reconstructing the older catalog lineage.

The change affects presentation data but still crosses the exact registry-hash save
boundary. Catalog-v16 and older saves remain incompatible and require a new game; no
save is silently converted and no validation gate is bypassed.