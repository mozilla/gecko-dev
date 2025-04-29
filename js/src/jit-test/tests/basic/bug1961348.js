try {
    evaluate(`os.file.listDirRelativeToScript("alksjdf12dsadyz330");`,
             {fileName: "a".repeat(10000)});
} catch {}
