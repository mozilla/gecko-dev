gcparam('nurseryEnabled', false);
gczeal(17);
transplantableObject().transplant(newGlobal());
gcparam('nurseryEnabled', true);
gc();
