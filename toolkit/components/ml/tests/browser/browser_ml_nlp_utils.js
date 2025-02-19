/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { CountVectorizer, CTfIdf, KeywordExtractor, cosSim, matMul } =
  ChromeUtils.importESModule("chrome://global/content/ml/NLPUtils.sys.mjs");

const { ENGLISH_STOP_WORDS } = ChromeUtils.importESModule(
  "chrome://global/content/ml/StopWords.sys.mjs"
);

add_task(function test_matmul_single_element_matrices() {
  const A = [[3]];
  const B = [[4]];
  const C = [[12]];
  Assert.deepEqual(matMul(A, B), C);
});

add_task(function test_matmul_identity_matrix() {
  const A = [
    [1, 0],
    [0, 1],
  ];
  const B = [
    [5, 6],
    [7, 8],
  ];
  Assert.deepEqual(matMul(A, B), B);
});

add_task(function test_matmul_zero_matrix() {
  const A = [
    [0, 0],
    [0, 0],
  ];
  const B = [
    [2, 3],
    [4, 5],
  ];
  Assert.deepEqual(matMul(A, B), A);
});

add_task(function test_matmul_two_element_matrices() {
  const A = [
    [1, 2],
    [3, 4],
  ];
  const B = [
    [5, 6],
    [7, 8],
  ];
  const C = [
    [19, 22],
    [43, 50],
  ];
  Assert.deepEqual(matMul(A, B), C);
});

add_task(function test_matmul_two_rectangular_matrices() {
  const A = [
    [1, 2, 3],
    [4, 5, 6],
  ];
  const B = [
    [7, 8],
    [9, 10],
    [11, 12],
  ];
  const C = [
    [58, 64],
    [139, 154],
  ];
  Assert.deepEqual(matMul(A, B), C);
});

add_task(function test_matmul_dimensional_mismatch() {
  const A = [
    [1, 2, 3],
    [4, 5, 6],
  ];
  const B = [
    [1, 2],
    [3, 4],
  ];
  Assert.throws(() => matMul(A, B), /Error/);
});

add_task(function test_matmul_negative_matrices() {
  const A = [
    [-1, 2],
    [3, -4],
  ];
  const B = [
    [2, -3],
    [-1, 4],
  ];
  const C = [
    [-4, 11],
    [10, -25],
  ];
  Assert.deepEqual(matMul(A, B), C);
});

add_task(function test_matmul_floating_point_matrices() {
  const A = [
    [0.5, 1.2],
    [2.1, 3.0],
  ];
  const B = [
    [1.0, 2.0],
    [0.5, 1.5],
  ];
  const C = [
    [1.1, 2.8],
    [3.6, 8.7],
  ];
  Assert.deepEqual(matMul(A, B), C);
});

add_task(async function test_cos_sim() {
  const a = [1, 1];
  const b = [1, 1, 1, 1, 1];
  const c = [0, 0, 0, 0, 0];
  const d = [-1, -1];

  // test similarity
  Assert.ok(isEqualWithTolerance(cosSim(a, a), 1));
  Assert.ok(isEqualWithTolerance(cosSim(b, b), 1));
  Assert.ok(isEqualWithTolerance(cosSim([], []), 0));
  Assert.ok(isEqualWithTolerance(cosSim(b, c), 0));
  Assert.ok(isEqualWithTolerance(cosSim(a, d), -1));
  Assert.ok(
    isEqualWithTolerance(cosSim([1, 2, 3, 4, 5], [5, 4, 3, 2, 1]), 0.63636364)
  );

  // test errors
  const expectedError = /Error/;
  Assert.throws(() => cosSim(a, []), expectedError);
  Assert.throws(() => cosSim([], a), expectedError);
});

add_task(async function test_count_vectorizer_stop_words() {
  const cv1 = new CountVectorizer();
  const cv2 = new CountVectorizer("EN");
  const cv3 = new CountVectorizer("invalid");
  Assert.greater(
    cv1.stopWords.size,
    0,
    "default English stop words should be present"
  );
  Assert.greater(cv2.stopWords.size, 0, "English stop words should be present");
  Assert.equal(
    cv3.stopWords.size,
    0,
    "invalid stop words should not be present"
  );
});

add_task(async function test_count_vectorizer_tokenization() {
  const cv = new CountVectorizer();
  const corpus = [
    "Hello there",
    "Hello, there",
    "Hello, there.",
    "Hello there?",
    "the quick brown fox jumps over the lazy dog",
  ];
  Assert.deepEqual(
    corpus.map(doc => cv.tokenize(doc)),
    [
      ["hello"],
      ["hello"],
      ["hello", ""],
      ["hello", ""],
      ["quick", "brown", "fox", "jumps", "lazy", "dog"],
    ],
    "Tokenized docs should be lower case, have no punctuations and have stopwords removed"
  );
});

add_task(async function test_count_vectorizer_fit() {
  const cv = new CountVectorizer();
  const corpus = [
    "Planning a trip",
    "Travel cost for vacation",
    "Places to visit",
    "Planning a trip",
  ];
  cv.fit(corpus);
  const vtoIdx = {
    planning: 0,
    trip: 1,
    travel: 2,
    cost: 3,
    vacation: 4,
    places: 5,
    visit: 6,
  };
  for (let vocab of Object.keys(cv.vocabToIdx)) {
    Assert.equal(
      vtoIdx[vocab],
      cv.vocabToIdx[vocab],
      "Vocab and indices should be the same"
    );
  }
});

add_task(async function test_count_vectorizer_transform() {
  const cv = new CountVectorizer();
  const corpus = [
    "Planning a trip",
    "Travel cost for vacation",
    "Places to visit",
    "Planning to visit and planning to travel",
  ];
  const corpusIdx = [
    [1, 1, 0, 0, 0, 0, 0],
    [0, 0, 1, 1, 1, 0, 0],
    [0, 0, 0, 0, 0, 1, 1],
    [2, 0, 1, 0, 0, 0, 1],
  ];
  cv.fit(corpus);
  const transformedCorpus = cv.transform(corpus);
  for (let i = 0; i < transformedCorpus.length; i++) {
    Assert.deepEqual(
      transformedCorpus[i],
      corpusIdx[i],
      "Counts should be the same"
    );
  }
});

add_task(async function test_count_vectorizer_fit_transform() {
  const cv = new CountVectorizer();
  const corpus = [
    "Planning a trip",
    "Travel cost for vacation",
    "Places to visit",
    "Planning to visit and planning to travel",
  ];
  const corpusIdx = [
    [1, 1, 0, 0, 0, 0, 0],
    [0, 0, 1, 1, 1, 0, 0],
    [0, 0, 0, 0, 0, 1, 1],
    [2, 0, 1, 0, 0, 0, 1],
  ];
  const transformedCorpus = cv.fitTransform(corpus);
  for (let i = 0; i < transformedCorpus.length; i++) {
    Assert.deepEqual(
      transformedCorpus[i],
      corpusIdx[i],
      "Counts should be the same"
    );
  }
});

add_task(async function test_count_vectorizer_get_feature_names() {
  const cv = new CountVectorizer();
  const corpus = [
    "Planning a trip",
    "Travel cost for vacation",
    "Places to visit",
    "Planning to visit and planning to travel",
  ];
  cv.fit(corpus);
  Assert.deepEqual(cv.getFeatureNamesOut(), [
    "planning",
    "trip",
    "travel",
    "cost",
    "vacation",
    "places",
    "visit",
  ]);
});

add_task(async function test_ctf_idf_diagonal_matrix() {
  const cti = new CTfIdf();
  Assert.deepEqual(cti.createDiagonalMatrix([1]), [[1]]);
  Assert.deepEqual(cti.createDiagonalMatrix([1, 2, 3]), [
    [1, 0, 0],
    [0, 2, 0],
    [0, 0, 3],
  ]);
});

add_task(async function test_ctf_idf_normalize() {
  const cti = new CTfIdf();
  const X = [
    [1, 1, 0, 0, 0, 0, 0],
    [0, 0, 1, 1, 1, 0, 0],
    [0, 0, 0, 0, 0, 1, 1],
    [2, 0, 1, 0, 0, 0, 1],
  ];
  const normalizedX = [
    [0.5, 0.5, 0, 0, 0, 0, 0],
    [0, 0, 0.3333333333333333, 0.3333333333333333, 0.3333333333333333, 0, 0],
    [0, 0, 0, 0, 0, 0.5, 0.5],
    [0.5, 0, 0.25, 0, 0, 0, 0.25],
  ];
  Assert.deepEqual(cti.normalize(X), normalizedX);
  Assert.deepEqual(cti.normalize([[1]]), [[1]]);
});

add_task(async function test_ctf_idf_fit() {
  const cti = new CTfIdf();
  const X = [
    [1, 1, 0, 0, 0, 0, 0],
    [0, 0, 1, 1, 1, 0, 0],
    [0, 0, 0, 0, 0, 1, 1],
    [2, 0, 1, 0, 0, 0, 1],
  ];
  const idfDiag = [
    [0.5108256237659906, 0, 0, 0, 0, 0, 0],
    [0, 1.0986122886681096, 0, 0, 0, 0, 0],
    [0, 0, 0.6931471805599453, 0, 0, 0, 0],
    [0, 0, 0, 1.0986122886681096, 0, 0, 0],
    [0, 0, 0, 0, 1.0986122886681096, 0, 0],
    [0, 0, 0, 0, 0, 1.0986122886681096, 0],
    [0, 0, 0, 0, 0, 0, 0.6931471805599453],
  ];
  cti.fit(X);
  Assert.deepEqual(cti.idfDiag, idfDiag);
});

add_task(async function test_ctf_idf_transform() {
  const cti = new CTfIdf();
  const X = [
    [1, 1, 0, 0, 0, 0, 0],
    [0, 0, 1, 1, 1, 0, 0],
    [0, 0, 0, 0, 0, 1, 1],
    [2, 0, 1, 0, 0, 0, 1],
  ];
  const transformedX = [
    [0.2554128118829953, 0.5493061443340548, 0, 0, 0, 0, 0],
    [0, 0, 0.23104906018664842, 0.36620409622270317, 0.36620409622270317, 0, 0],
    [0, 0, 0, 0, 0, 0.5493061443340548, 0.34657359027997264],
    [0.2554128118829953, 0, 0.17328679513998632, 0, 0, 0, 0.17328679513998632],
  ];
  cti.fit(X);
  Assert.deepEqual(cti.transform(X), transformedX);
});

add_task(async function test_ctf_idf_fit_transform() {
  const cti = new CTfIdf();
  const X = [
    [1, 1, 0, 0, 0, 0, 0],
    [0, 0, 1, 1, 1, 0, 0],
    [0, 0, 0, 0, 0, 1, 1],
    [2, 0, 1, 0, 0, 0, 1],
  ];
  const transformedX = [
    [0.2554128118829953, 0.5493061443340548, 0, 0, 0, 0, 0],
    [0, 0, 0.23104906018664842, 0.36620409622270317, 0.36620409622270317, 0, 0],
    [0, 0, 0, 0, 0, 0.5493061443340548, 0.34657359027997264],
    [0.2554128118829953, 0, 0.17328679513998632, 0, 0, 0, 0.17328679513998632],
  ];
  Assert.deepEqual(cti.fitTransform(X), transformedX);
});

add_task(async function test_extract_keywords_single_document() {
  const corpus = [
    "Planning a trip to Boston. Boston duck tours. Music in Boston. Flights to Boston. Planning trip back after.",
  ];
  const keywordList = [["boston", "planning", "trip", "duck", "tours"]];
  const keywordExtractor = new KeywordExtractor();
  Assert.deepEqual(keywordExtractor.fitTransform(corpus, 5), keywordList);
});

add_task(async function test_extract_keywords_unique_keywords_per_document() {
  const corpus = [
    "Planning a trip to Boston. Boston duck tours. Music in Boston. Flights to Boston",
    "Planning a trip to Brazil. Flights to Brazil. Beach tour. More Planning.",
    "Planning dinner tonight. Brussel Sprouts. Meal Planning",
  ];
  const keywordList = [
    ["boston", "duck", "tours"],
    ["brazil", "beach", "tour"],
    ["dinner", "tonight", "brussel"],
  ];
  const keywordExtractor = new KeywordExtractor();

  Assert.deepEqual(keywordExtractor.fitTransform(corpus), keywordList);
});
