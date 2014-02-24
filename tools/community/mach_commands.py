# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os, sys, subprocess, re
from collections import Counter

from mach.decorators import (
    CommandProvider,
    CommandArgument,
    Command,
)

from mozbuild.base import MachCommandBase

history = 1
number = 1

@CommandProvider
class FindReviewerCommand(object):
    def __init__(self, context):
        self._context = context

    @Command('find-reviewer', category='misc',
        description='Find the right reviewer for your patch.\n' + '')

    @CommandArgument('--number','-n', default='1', type=int,
        help='Recommend a number of reviewers, in order of likelihood. Default is the 1 most likely reviewer.')
    @CommandArgument('--history','-h', default='8', type=int,
        help='Look back for some number of prior reviewers for each file. Default is 8.')

    # These may or may not be consistent over time, so:
    def find_reviewers(self, number, history):
            
        GetModifiedFiles_Git = "git ls-files -m "
        GetPreviousCheckins_Git = "git log -n " + str(history) + " " 
        GetSummary_Git = "git log --pretty=oneline --abbrev-commit -n " + str(history) + " " 

        GetModifiedFiles_Hg = "hg status -n --modified"
        GetPreviousCheckins_Hg = "hg log -l " + str(history) + " " 
        GetSummary_Hg          = "hg log --template '{desc|firstline}\\n' -l " + str(history) + " "  

       
        def GetReviewersFromGit(CanRevs):
            ChangeSets = subprocess.check_output(GetModifiedFiles_Git.split(),
                              shell=False, universal_newlines=True).split("\n")
            for FileChanged in ChangeSets:
                GetChangeHistory = GetPreviousCheckins_Git + str(FileChanged)
                PerFileChangeHistory = subprocess.check_output(GetChangeHistory.split(),
                                        shell=False, universal_newlines=True).split("\n")
                for Words in PerFileChangeHistory:
                    if "r=" in Words:
                        CanRevs.append(Words[Words.rfind("r=")+2:])
            return CanRevs

        def GetReviewersFromHg(CanRevs):
            Changesets = subprocess.check_output(GetModifiedFiles_Hg.split(), 
                                                  shell=False, universal_newlines=True).split("\n")
            for FileChanged in Changesets:
                GetChangeHistory = GetSummary_Hg + str(FileChanged)
                PerFileChangeHistory = subprocess.check_output(GetChangeHistory.split(), 
                                                  shell=False, universal_newlines=True).split("\n")
                for Words in PerFileChangeHistory:
                    if "r=" in Words:
                        CanRevs.append(Words[Words.rfind("r=")+2:])
            return CanRevs

        def GetReviewers():
            CandidateReviewers = list() 
            if os.path.exists(".hg"):   
                return GetReviewersFromHg(CandidateReviewers)
            else:
                return GetReviewersFromGit(CandidateReviewers)
            return Counter(CandidateReviewers)

        if number == 1:
            print ("The most likely reviewer for your patch is: " 
                    + " ".join(GetReviewers()[:1])  )
        else:
            print ("It's likely that the reviewers for your patch are: " 
                    + " ".join(GetReviewers()[:number])  )


# This fails in a number of important ways - "r=gps." != "r=gps" for example, or "r=me".
# But improvements are, as ever, on the way.

