// |jit-test| error: Error

a = String.fromCharCode(-10);
os.file.listDir(a);

