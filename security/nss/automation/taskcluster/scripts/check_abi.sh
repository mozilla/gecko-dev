#! /bin/bash

set_env()
{
  OUTPUTDIR=$(pwd)$(echo "/output")
  DATE=$(date "+TB [%Y-%m-%d %H:%M:%S]")

  if [ ! -d "${OUTPUTDIR}" ]; then
    echo "Creating output dir"
    mkdir "${OUTPUTDIR}"
  fi

  cp -a ${VCS_PATH}/nss ${VCS_PATH}/nspr .
  pushd nspr
  if [[ -f ../nss/nspr.patch && "$ALLOW_NSPR_PATCH" == "1" ]]; then
    cat ../nss/nspr.patch | patch -p1
  fi
  popd

  cd nss
  ./build.sh -v -c --python=python3
  cd ..
}

check_abi()
{
  set_env
  if [[ "$1" != --nobuild ]]; then # Start nobuild block

    echo "######## NSS ABI CHECK ########"
    echo "######## creating temporary HG clones ########"

    rm -rf baseline
    mkdir baseline
    BASE_NSS=`cat nss/automation/abi-check/previous-nss-release`  #Reads the version number of the last release from the respective file
    if ! hg clone -u "${BASE_NSS}" "${VCS_PATH}/nss" baseline/nss; then
      echo "invalid tag in automation/abi-check/previous-nss-release"
      return 1
    fi

    BASE_NSPR=NSPR_$(head -1 baseline/nss/automation/release/nspr-version.txt | cut -d . -f 1-2 | tr . _)_BRANCH
    if ! hg clone -u "${BASE_NSPR}" "${VCS_PATH}/nspr" baseline/nspr; then
      rm -rf baseline/nspr
      hg clone -u "default" "${VCS_PATH}/nspr" baseline/nspr
      echo "Nonexisting tag ${BASE_NSPR} derived from ${BASE_NSS} automation/release/nspr-version.txt"
      echo "Using default branch instead."
    fi

    echo "######## building baseline NSPR/NSS ########"
    echo "${PWD}/baseline/nss/build.sh"
    cd baseline/nss
    ./build.sh -v -c --python=python3
    cd -
  else  # Else nobuild block
    echo "######## using existing baseline NSPR/NSS build ########"
  fi # End nobuild block

  echo "######## Starting abidiff procedure ########"
  abi_diff
}

#Slightly modified from build.sh in this directory
abi_diff()
{
  ABI_PROBLEM_FOUND=0
  ABI_REPORT=${OUTPUTDIR}/abi-diff.txt
  rm -f ${ABI_REPORT}
  PREVDIST=baseline/dist
  NEWDIST=dist
  # libnssdbm3.so isn't built by default anymore, skip it.
  ALL_SOs="libfreebl3.so libfreeblpriv3.so libnspr4.so libnss3.so libnssckbi.so libnsssysinit.so libnssutil3.so libplc4.so libplds4.so libsmime3.so libsoftokn3.so libssl3.so"
  for SO in ${ALL_SOs}; do
      if [ ! -f nss/automation/abi-check/expected-report-$SO.txt ]; then
          touch nss/automation/abi-check/expected-report-$SO.txt
      fi
      abidiff --hd1 $PREVDIST/public/ --hd2 $NEWDIST/public \
          $PREVDIST/*/lib/$SO $NEWDIST/*/lib/$SO \
          > nss/automation/abi-check/new-report-temp$SO.txt
      RET=$?
      cat nss/automation/abi-check/new-report-temp$SO.txt \
          | grep -v "^Functions changes summary:" \
          | grep -v "^Variables changes summary:" \
          | sed -e 's/__anonymous_enum__[0-9]*/__anonymous_enum__/g' \
          > nss/automation/abi-check/new-report-$SO.txt
      rm -f nss/automation/abi-check/new-report-temp$SO.txt

      ABIDIFF_ERROR=$((($RET & 0x01) != 0))
      ABIDIFF_USAGE_ERROR=$((($RET & 0x02) != 0))
      ABIDIFF_ABI_CHANGE=$((($RET & 0x04) != 0))
      ABIDIFF_ABI_INCOMPATIBLE_CHANGE=$((($RET & 0x08) != 0))
      ABIDIFF_UNKNOWN_BIT_SET=$((($RET & 0xf0) != 0))

      # If abidiff reports an error, or a usage error, or if it sets a result
      # bit value this script doesn't know yet about, we'll report failure.
      # For ABI changes, we don't yet report an error. We'll compare the
      # result report with our allowlist. This allows us to silence changes
      # that we're already aware of and have been declared acceptable.

      REPORT_RET_AS_FAILURE=0
      if [ $ABIDIFF_ERROR -ne 0 ]; then
          echo "abidiff reported ABIDIFF_ERROR."
          REPORT_RET_AS_FAILURE=1
      fi
      if [ $ABIDIFF_USAGE_ERROR -ne 0 ]; then
          echo "abidiff reported ABIDIFF_USAGE_ERROR."
          REPORT_RET_AS_FAILURE=1
      fi
      if [ $ABIDIFF_UNKNOWN_BIT_SET -ne 0 ]; then
          echo "abidiff reported ABIDIFF_UNKNOWN_BIT_SET."
          REPORT_RET_AS_FAILURE=1
      fi

      if [ $ABIDIFF_ABI_CHANGE -ne 0 ]; then
          echo "Ignoring abidiff result ABI_CHANGE, instead we'll check for non-allowlisted differences."
      fi
      if [ $ABIDIFF_ABI_INCOMPATIBLE_CHANGE -ne 0 ]; then
          echo "Ignoring abidiff result ABIDIFF_ABI_INCOMPATIBLE_CHANGE, instead we'll check for non-allowlisted differences."
      fi

      if [ $REPORT_RET_AS_FAILURE -ne 0 ]; then
          ABI_PROBLEM_FOUND=1
          echo "abidiff {$PREVDIST , $NEWDIST} for $SO FAILED with result $RET, or failed writing to nss/automation/abi-check/new-report-$SO.txt"
      fi
      if [ ! -f nss/automation/abi-check/expected-report-$SO.txt ]; then
          ABI_PROBLEM_FOUND=1
          echo "FAILED to access report file: nss/automation/abi-check/expected-report-$SO.txt"
      fi

      diff -wB -u nss/automation/abi-check/expected-report-$SO.txt \
              nss/automation/abi-check/new-report-$SO.txt >> ${ABI_REPORT}
      if [ ! -f ${ABI_REPORT} ]; then
          ABI_PROBLEM_FOUND=1
          echo "FAILED to compare exepcted and new report: nss/automation/abi-check/new-report-$SO.txt"
      fi
  done

  if [ -s ${ABI_REPORT} ]; then
      echo "FAILED: there are new unexpected ABI changes"
      cat ${ABI_REPORT}
      return 1
  elif [ $ABI_PROBLEM_FOUND -ne 0 ]; then
      echo "FAILED: failure executing the ABI checks"
      cat ${ABI_REPORT}
      return 1
  fi

  return 0
}

check_abi $1
