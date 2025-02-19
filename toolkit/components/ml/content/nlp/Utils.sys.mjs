/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ENGLISH_STOP_WORDS } from "chrome://global/content/ml/StopWords.sys.mjs";

// max number of keywords to extract for each document in corpus
const MAX_WORDS_PER_DOCUMENT = 3;

// min threshold to extract a keyword from document in corpus
const MIN_THRESHOLD_FOR_KEYWORD = 0.05;

/**
 * A simple implementation of matrix multiplication
 *
 * @param {float [][]} A 2D matrix
 * @param {float [][]} B 2D matrix
 * @returns {float [][]} 2D matrix multiplication result
 */
export function matMul(A, B) {
  // Check if multiplication is possible (i.e., number of columns in A == number of rows in B)
  if (!A || !B || A[0].length !== B.length) {
    throw new Error("Matrix dimensions are incompatible for multiplication.");
  }

  // Create the result matrix with appropriate dimensions (rows of A, columns of B)
  const result = [];
  for (let i = 0; i < A.length; i++) {
    result[i] = [];
    for (let j = 0; j < B[0].length; j++) {
      let sum = 0;
      // Sum the product of corresponding elements
      for (let k = 0; k < A[0].length; k++) {
        sum += A[i][k] * B[k][j];
      }
      result[i][j] = sum;
    }
  }

  return result;
}

/**
 * Calculates cosine similarity between two lists of floats
 * The lists don't need to be normalized
 *
 * @param {number []} A first list
 * @param {number []} B second list
 * @returns {number} cosine similarity value
 */
export function cosSim(A, B) {
  if (!A || !B || A.length !== B.length) {
    throw new Error("Lists should have same lengths and not be undefined");
  }
  if (A.length === 0 && A.length === B.length) {
    return 0;
  }

  let dotProduct = 0;
  let mA = 0;
  let mB = 0;

  for (let i = 0; i < A.length; i++) {
    dotProduct += A[i] * B[i];
    mA += A[i] * A[i];
    mB += B[i] * B[i];
  }

  mA = Math.sqrt(mA);
  mB = Math.sqrt(mB);

  return mA === 0 || mB === 0 ? 0 : dotProduct / (mA * mB);
}

/**
 * This class is used to generate a vectorized count of keywords
 * that are not part of the chosen language stopwords
 */
export class CountVectorizer {
  constructor(stopWords = "EN") {
    this.stopWords = new Set(stopWords === "EN" ? ENGLISH_STOP_WORDS : []);
  }

  /**
   * Tokenize document based on non-stopwords
   *
   * @param {string} doc String to tokenize
   * @returns {string[]} List containing token strings
   */
  tokenize(doc) {
    return doc
      .replace(/[0-9]/g, "") // remove numbers
      .split(/\W+/) // split on any non-word character
      .map(token => token.toLowerCase()) // convert to lowercase
      .filter(word => !this.stopWords.has(word)); // remove stop-words
  }

  /**
   * Fit to corpus and generate the vocabulary
   *
   * @param {string []} corpus list of text to fit vectorizer to
   */
  fit(corpus) {
    this.tokenizedCorpus = corpus.map(doc => this.tokenize(doc));
    const vocabSet = new Set();
    for (let doc of this.tokenizedCorpus) {
      for (let token of doc) {
        if (token) {
          vocabSet.add(token);
        }
      }
    }
    const vocab = Array.from(vocabSet);
    this.vocabToIdx = {};
    this.vocabLength = vocab.length;
    for (let i = 0; i < vocab.length; i++) {
      this.vocabToIdx[vocab[i]] = i;
    }
  }

  /**
   * Transform tokenized text based on non stop-word characters from vocabulary
   *
   * @param {string [][]} tokenizedCorpus list of tokenized strings to transform
   * @returns {float [][]} Mapped token counts per string doc
   */
  #transformTokenized(tokenizedCorpus) {
    return tokenizedCorpus.map(doc => {
      const counts = new Array(this.vocabLength).fill(0);
      for (let token of doc) {
        if (token in this.vocabToIdx) {
          counts[this.vocabToIdx[token]]++;
        }
      }
      return counts;
    });
  }

  /**
   * Transform text based on existing vocabulary
   *
   * @param {string []} corpus list of string to transform
   * @returns {float [][]} transformed counts based on tokens
   */
  transform(corpus) {
    const tokenizedCorpus =
      this.tokenizedCorpus || corpus.map(doc => this.tokenize(doc));
    return this.#transformTokenized(tokenizedCorpus);
  }

  /**
   * Fit and transform text based on existing vocabulary
   *
   * @param {string []} corpus list of string to transform
   * @returns {float [][]} transformed counts based on tokens
   */
  fitTransform(corpus) {
    this.fit(corpus);
    return this.#transformTokenized(this.tokenizedCorpus);
  }

  /**
   * Extract the most important keywords from corpus
   *
   * @returns {string []} list of keywords
   */
  getFeatureNamesOut() {
    return Object.keys(this.vocabToIdx);
  }
}

/*
 * Simplified ctf-idf implementation based off of
 * https://github.com/MaartenGr/BERTopic/blob/master/bertopic/vectorizers/_ctfidf.py
 * https://maartengr.github.io/BERTopic/api/ctfidf.html
 *
 * This class identifies keywords important to clusters of documents
 * by assigning scores to each keyword in the cluster relative to all clusters
 * present in the corpus
 */
export class CTfIdf {
  /**
   * Generate diagonal matrix using 1D array
   *
   * @param {float []} array 1D array to create a diagonalized matrix for
   * @returns {float [][]} 2D matrix with diagonal values
   */
  createDiagonalMatrix(array) {
    const n = array.length; // Length of the input array
    const matrix = Array.from({ length: n }, () => Array(n).fill(0)); // Create an n x n matrix filled with 0

    for (let i = 0; i < n; i++) {
      matrix[i][i] = array[i]; // Place elements of the array along the main diagonal
    }
    return matrix;
  }

  /**
   * Normalize 2D array along the columns
   *
   * @param {float [][]} array 2D matrix to be normalized
   * @returns {float [][]} 2D matrix with normalized values by row
   */
  normalize(array) {
    // Perform normalization row-wise (axis=1)
    for (let i = 0; i < array.length; i++) {
      const row = array[i];
      // Calculate the L1 norm (sum of absolute values)
      const rowSum = row.reduce((sum, value) => sum + Math.abs(value), 0);

      if (rowSum !== 0) {
        // Prevent division by zero
        for (let j = 0; j < row.length; j++) {
          row[j] = row[j] / rowSum; // Normalize each element in the row
        }
      }
    }
    return array;
  }

  /**
   * Learn the "idf" portion (global term weights)
   * by fitting to the count-vectorized array
   *
   * @param {float [][]} array A matrix of term/token counts.
   */
  fit(array) {
    if (array.length === 0 || array[0].length === 0) {
      throw new Error("Non-Empty 2D array is required");
    }
    const df = array[0].map((_, colIndex) =>
      array.reduce((sum, row) => sum + row[colIndex], 0)
    );
    const avgNrSamples = Math.floor(
      array.reduce(
        (total, row) => total + row.reduce((sum, value) => sum + value, 0),
        0
      ) / array.length
    );
    const idf = df.map(value => Math.log(avgNrSamples / value + 1));
    this.idfDiag = this.createDiagonalMatrix(idf);
  }

  /**
   * Find the most important keywords based off the previously
   * fitted clusters in the corpus
   *
   * @param {float [][]} array 2D matrix to transform vectorized matrix
   * @returns {float [][]} transformed matrix
   */
  transform(array) {
    const X = this.normalize(array);
    return matMul(X, this.idfDiag);
  }

  /**
   * Apply the fit and transform operations on the same corpus
   *
   * @param {float [][]} array 2D matrix to fit vectorized matrix
   * @returns {float [][]} transformed matrix
   */
  fitTransform(array) {
    this.fit(array);
    return this.transform(array);
  }
}

/**
 * This class extracts unique keywords from a corpus of documents
 * It currently uses the CountVectorizer and CtfIdf classes but can
 * be extended as more methods are added
 */
export class KeywordExtractor {
  constructor() {
    this.cv = new CountVectorizer();
    this.ctfIdf = new CTfIdf();
  }

  /**
   * Generate the CtfIdf vector of important keywords and their scores
   * and return the n most important keywords
   *
   * @param {string []} corpus
   * @param {int} n max number of keywords to extract
   * @returns {string[][]}
   */
  fitTransform(corpus, n = MAX_WORDS_PER_DOCUMENT) {
    const transformedCorpus = this.cv.fitTransform(corpus);
    this.transformedCtfIdf = this.ctfIdf.fitTransform(transformedCorpus);
    const { sortedScores, sortedIndices } = this.#getSortedScoresForKeywords();
    return this.#getKeywordsPerCluster(sortedIndices, sortedScores, n);
  }

  /**
   * Generate the sorted keywords along with their corresponding scores
   *
   * @returns {{sortedScores: float [][], sortedIndices: float [][]}}
   */
  #getSortedScoresForKeywords() {
    const sortedIndices = this.transformedCtfIdf.map(row => {
      return row
        .map((value, index) => ({ value, index })) // Pair each value with its index
        .sort((a, b) => b.value - a.value) // Sort in descending order
        .map(item => item.index); // Extract sorted indices
    });
    const sortedScores = this.transformedCtfIdf.map((row, clusterIndex) => {
      return sortedIndices[clusterIndex].map(index => row[index]);
    });
    return { sortedIndices, sortedScores };
  }

  /**
   * For each cluster of documents in the corpus, extract the important keywords
   *
   * @param {float [][]} sortedIndices
   * @param {float [][]} sortedScores
   * @param {int} n max number of keywords to extract
   * @returns {string [][]}
   */
  #getKeywordsPerCluster(sortedIndices, sortedScores, n) {
    const numClusters = this.transformedCtfIdf.length;
    const numWords = this.transformedCtfIdf[0].length;
    const keywords = this.cv.getFeatureNamesOut();

    const keywordsForCluster = [];
    for (let cluster = 0; cluster < numClusters; cluster++) {
      let topicWords = [];

      // fetch the important keywords for each document
      for (let topScoreRef = 0; topScoreRef < numWords; topScoreRef++) {
        // break if we find a score less than the minimum threshold since the array is "score-sorted"
        // also break if we have reach the limit number of keywords to return
        if (
          sortedScores[cluster][topScoreRef] < MIN_THRESHOLD_FOR_KEYWORD ||
          topicWords.length >= n
        ) {
          break;
        }
        const wordIndex = sortedIndices[cluster][topScoreRef];
        topicWords.push(keywords[wordIndex]);
      }

      // Add the topic words to the list
      keywordsForCluster.push(topicWords);
    }
    return keywordsForCluster;
  }
}
