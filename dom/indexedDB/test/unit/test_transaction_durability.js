/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* exported testSteps */
async function testSteps() {
  const name = "test_transaction_durability";
  const abc = "abcdefghijklmnopqrstuvwxyz";
  const durabilities = ["relaxed", "default", "strict"];

  // Repeat the measurement 3 times.
  const measurementCount = 3;

  // The difference between relaxed and default is negligible (especially with
  // SSDs).
  const relaxedDefaultDiffPercentage = 1;

  // The difference between default and strict is notable (even with SSDs).
  const defaultStrictDiffPercentage = 20;

  // Allow some tolerance (to deal with noise).
  const tolerancePercentage = (function () {
    if (isInChaosMode()) {
      return 50;
    }

    return 30;
  })();

  // Adjust the number of transactions, so the test takes roughly same time
  // across all platforms (including the chaos mode).
  const transactionCount = (function () {
    if (isInChaosMode()) {
      switch (mozinfo.os) {
        case "linux":
          return 50;

        case "mac":
          return 7;

        case "win":
          return 650;

        case "android":
          return 1;

        default:
          return 250;
      }
    }

    switch (mozinfo.os) {
      case "linux":
        return 275;

      case "mac":
        return 1150;

      case "win":
        return 650;

      case "android":
        return 135;

      default:
        return 500;
    }
  })();

  // A helper function (could be moved to a common place).
  function transposeMatrix(matrix) {
    for (let i = 0; i < matrix.length; i++) {
      for (let j = 0; j < i; j++) {
        const tmp = matrix[i][j];
        matrix[i][j] = matrix[j][i];
        matrix[j][i] = tmp;
      }
    }
  }

  // A helper function (could be moved to a common place).
  function getMedian(array) {
    array.sort((a, b) => a - b);

    const middleIndex = Math.floor(array.length / 2);

    if (array.length % 2 === 0) {
      return (array[middleIndex - 1] + array[middleIndex]) / 2;
    }

    return array[middleIndex];
  }

  // Data generation.
  async function createDatabase(actualName) {
    info(`Creating database ${actualName}`);

    const request = indexedDB.open(actualName, 1);

    const event = await expectingUpgrade(request);

    const database = event.target.result;

    database.createObjectStore(name);

    await expectingSuccess(request);

    return database;
  }

  async function fillDatabase(database, durability) {
    return new Promise(function (resolve) {
      const startTime = Cu.now();

      info(`Filling database ${database.name} using ${durability} durability`);

      let index = 0;

      function addData() {
        const transaction = database.transaction(name, "readwrite", {
          durability,
        });

        const objectStore = transaction.objectStore(name);

        objectStore.add(abc, index++);

        transaction.oncomplete = function () {
          if (index < transactionCount) {
            addData();
          } else {
            const endTime = Cu.now();

            const timeDelta = endTime - startTime;

            info(
              `Filled database ${database.name} using ${durability} ` +
                `durability in ${timeDelta} msec`
            );

            resolve(timeDelta);
          }
        };
      }

      addData();
    });
  }

  const timeDeltaMatrix = await (async function () {
    let timeDeltaMatrix = [];

    for (
      let measurementIndex = 0;
      measurementIndex < measurementCount;
      measurementIndex++
    ) {
      let databases = [];

      for (
        let durabilityIndex = 0;
        durabilityIndex < durabilities.length;
        durabilityIndex++
      ) {
        const actualName =
          name + "_" + measurementIndex + "_" + durabilityIndex;

        const database = await createDatabase(actualName);

        databases.push(database);
      }

      let promises = [];

      for (
        let durabilityIndex = 0;
        durabilityIndex < durabilities.length;
        durabilityIndex++
      ) {
        const promise = fillDatabase(
          databases[durabilityIndex],
          durabilities[durabilityIndex]
        );

        promises.push(promise);
      }

      const timeDeltas = await Promise.all(promises);

      timeDeltaMatrix.push(timeDeltas);
    }

    // Convert rows to columns.
    transposeMatrix(timeDeltaMatrix);

    return timeDeltaMatrix;
  })();

  // Data evaluation.
  {
    let lastTimeDeltaMedian;

    for (
      let durabilityIndex = 0;
      durabilityIndex < durabilities.length;
      durabilityIndex++
    ) {
      const timeDeltaMedian = getMedian(timeDeltaMatrix[durabilityIndex]);

      info("Time delta median: " + timeDeltaMedian);

      if (lastTimeDeltaMedian) {
        const durability = durabilities[durabilityIndex];

        const actualTolerancePercentage =
          tolerancePercentage -
          (durability == "default"
            ? relaxedDefaultDiffPercentage
            : defaultStrictDiffPercentage);

        const coefficient = actualTolerancePercentage / 100;

        const adjustedTimeDeltaMedian =
          coefficient >= 0
            ? timeDeltaMedian * (1 + coefficient)
            : timeDeltaMedian / (1 + Math.abs(coefficient));

        Assert.greater(
          adjustedTimeDeltaMedian,
          lastTimeDeltaMedian,
          `Database filling using higher (${durability}) durability should ` +
            `take more time`
        );
      }

      lastTimeDeltaMedian = timeDeltaMedian;
    }
  }
}
