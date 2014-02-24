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
        description='Find the right reviewer for your patch. Recommends the one most likely reviewer\n' + '')

#    @CommandArgument(number, '--number','-n', default=1,
#        description='Recommend some number of reviewers, in order of likelihood. Default is the 1 most likely reviewer.')
#    @CommandArgument(history, '--history','-h', default=8, 
#        description='Look back for some number of prior reviewers for each file. Default is 8.')

    # These may or may not be consistent over time, so:
    def find_reviewers(self):
        GetModifiedFiles_Git = "git ls-files --modified "
        GetPreviousCheckins_Git = "git log -n 1" 
        GetSummary_Git = "git log --pretty=oneline --abbrev-commit -n " + str(history) 

        GetModifiedFiles_Hg = "hg status -n --modified"
        GetPreviousCheckins_Hg = "hg log -l 1 "
        GetSummary_Hg          = "hg log --template '{desc|firstline}\\n' -l 10 "   

       
        def GetReviewersFromGit(CanRevs):
            ChangeSet = subprocess.check_output(GetModifiedFiles_Git.split(), shell=False, universal_newlines=True).split("\n")
            for FileChanged in Changeset:
                GetChangeHistory = GetPreviousCheckins_Git + str(FileChanged)
                PerFileChangeHistory = subprocess.check_output(GetChangeHistory.split(), shell=False, universal_newlines=True).split("\n")
                for Words in PerFileChangeHistory:
                    if "r=" in Words:
                        CanRevs.append(Words[Words.rfind("r=")+2:])
                    

            return CanRevs

        def GetReviewersFromHg(CanRevs):
            Changesets = subprocess.check_output(GetModifiedFiles_Hg.split(), shell=False, universal_newlines=True).split("\n")
            for FileChanged in Changesets:
                GetChangeHistory = GetSummary_Hg + str(FileChanged)
                PerFileChangeHistory = subprocess.check_output(GetChangeHistory.split(), shell=False, universal_newlines=True).split("\n")
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

        print (Counter(GetReviewers())) 


