=======
Schemas
=======

There are two sets of JSON schemas used by the BackupService. One schema is used
for the ``backup-manifest.json`` file that gets inserted into a compressed backup
archive, and the other is for the JSON block that exists alongside the
compressed backup archive.

There is a similar ``meta`` property between both schemas. This property is for
the metadata about a backup (including information about the time of the backup,
and the machine / build that the backup was created on). There is admittedly
some redundancy here where this same metadata exists in the JSON block as well
as the backup manifest, but this redundancy helps to protect against accidental
(or intentional) corruption of a backup.

The drawback of this redundancy is that the schema for the metadata must be
maintained in lock-step between both the backup manifest and the JSON block.

This is done by defining the metadata structure separately within the
backup manifest schema, and _referring_ to that structure within both the
backup manifest schema and the JSON block schema.

This implies that the version numbers between both schemas are also maintained
in lock-step. This means that there is a global ``SCHEMA_VERSION`` defined inside
of ``ArchiveUtils`` that represents the current schema version for both the
backup manifest as well as the JSON block.

So this means that when you develop a new version of one schema, you must
also generate a new schema for the other using the same version number, even
if that second schema has not changed.
