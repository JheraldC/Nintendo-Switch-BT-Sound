# Reader provenance

The eight C/H reader files are frozen from the V4C.3 authenticated read-only
reader. core-freeze.json at the project root records their original SHA256;
build_payload.py and audit_auto.py verify them. Corresponding source is
included here, without embedded console data.

Platform integration derives from TegraExplorer-Ext 4.2.2, commit
80931b536860443ecca30ae1c25c55278a342b42. Vendored sources and licenses are
under vendor/tegraexplorer. Generic explorer/script/NAND-write functions are
excluded from the payload and checked by the ARM audit. Frozen original
integrity/filesystem files remain for regression comparison; production uses
the validated bridge/filesystem implementations in src as build evidence shows.
