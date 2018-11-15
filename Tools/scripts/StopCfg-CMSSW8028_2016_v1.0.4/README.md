# StopCfg

## Checkout Version of Cfg Files

See instructions here: https://github.com/susy2015/SusyAnaTools#check-out-stop-config-files

## Update Cfg Files

Here is an example for updating sampleSets.cfg with new weights.
For this example, we are changing from the SoftBjet_PhotonNtuples samples to the CMSSW8028_2016 samples.
- old path: /eos/uscms/store/user/lpcsusyhad/Stop_production/SoftBjet_PhotonNtuples/
- new path: /eos/uscms/store/user/lpcsusyhad/Stop_production/CMSSW8028_2016/

1. First create and copy new text files listing root files to EOS.

- Go to SusyAnaTools/Tools/condor area.

```cd $CMSSW_BASE/src/SusyAnaTools/Tools/condor```

- Run batchList.py (with -l for list, -c for copy to eos, and -d for the path to ntuples).

```python batchList.py -lc -d /eos/uscms/store/user/lpcsusyhad/Stop_production/CMSSW8028_2016```


2. Second replace file paths in sample text files.

- Get sample files and copy them to SusyAnaTools (if you don't already have them).

```
git clone git@github.com:susy2015/StopCfg.git
```

- Go to SusyAnaTools/Tools area.

```cd $CMSSW_BASE/src/SusyAnaTools/Tools```

- replace SoftBjet_PhotonNtuples with CMSSW8028_2016

```sed -i -e 's/SoftBjet_PhotonNtuples/CMSSW8028_2016/g' sampleSets.cfg```

3. Third compile SusyAnaTools and run nEvts with the output stored in a file.

- Go to SusyAnaTools/Tools area.

```cd $CMSSW_BASE/src/SusyAnaTools/Tools```

- Compile.

```make -j8```

- The script nEvts will take a long time to run (hours). You should use screen. Here are some useful screen commands.
  - To enter screen, use ```screen```.
  - To exit screen, use ```exit```.
  - To attach screen, use ```screen -r```.
  - To detach screen, use ```CRTL-A D```.
  - To list screens, use ```screen -ls```.

- When in screen, you are entering a new shell and a new environment. You will need to run some commands to obtain the desired environment. Bash users can begin with `source ~/.bash_profile`.

```
screen
source ~/.bash_profile
source /cvmfs/cms.cern.ch/cmsset_default.sh
cmsenv
source $CMSSW_BASE/src/TopTagger/TopTagger/test/taggerSetup.sh
```

Now run nEvts. We are redirecting stdout (1) to nEvents.txt and stderr (2) to nEvents_errors.log.
```
./nEvts -ws 1> nEvents.txt 2> nEvents_errors.log
```

You can also run over a specific sample such as `GJets_HT-200To400`, for example.
```
./nEvts -ws GJets_HT-200To400 1> nEvents.txt 2> nEvents_errors.log
```

4. Fourth run updateSamples.py with options (-e for output of nEvts, s for original cfg file, and -o for new cfg file).
```
python updateSamples.py -e nEvents.txt -s sampleSets.cfg -o sampleSets_v2.cfg > update.log
```

We redirect the output to update.log. Check update.log to see that each sample in sampleSets.cfg found exactly one match in nEvents.txt. The updateSamples.py script will produce sampleSets_v1.cfg (a copy of original sampleSets.cfg) and sampleSets_v2.cfg (the updated version of sampleSets.cfg).

5. Fifth make a new release.
- Download the repo if you have not already.
```
git clone git@github.com:susy2015/StopCfg.git
cd StopCfg
```
- Make a new branch based on the master branch.
```
git checkout -b <new_branch_name>
```
- Copy your updated version of the sample config files here.
- Add and commit your changes.
```
git commit -am "explanation of changes"
```
- Create a token on GitHub by going to Settings, Developer Settings, and finally Personal access tokens. Check the public_repo box. Make sure to copy and store the token that you create somewhere safe. It is only displayed once.
- Source a script that sets the GITHUB_TOKEN to the token that you created and saved. 
```
source ~/.ssh/tokens
```
This script should do the following.
```
#!/bin/bash
export GITHUB_TOKEN=<git_token_created_on_github>
```

- Add github-release and makeStopRelease.sh to your path. You make softlink to my versions.
```
mkdir ~/bin
ln -s /uscms/home/caleb/bin/github-release ~/bin/github-release
ln -s /uscms/home/caleb/bin/makeStopRelease.sh ~/bin/makeStopRelease.sh
export PATH="$PATH:~/bin"
```

- Use the makeStopRelease.sh script to publish a new release.
```
makeStopRelease.sh -h

Usage:
    makeStopRelease.sh -t RELEASE_TAG -b BRANCH [-d SUPP_FILE_DIR] [-m MESSAGE]

Options:
    -t RELEASE_TAG :       This is the github release tag which will be created (Required)
    -b BRANCH :            Git branch to base release off. This branch must exist. It will be pushed to github. (Required)
    -d SUPP_FILE_DIR :     The folder where the supplemental training file can be found (default supplementaryFiles)
    -m MESSAGE :           Commit message for tag (Default empty)

Description:
    This script automatically makes a release of the Stop search config files
    Run this script from the StopCfg directory
    Source a script that does "export GITHUB_TOKEN=<git_token_created_on_github>" before running this script.
```
Here is an example. The branch you use for the release must exist. Make sure that the tag and branch names are different.
Git does not like when branch and tag names are the same because then the names are ambiguous. The tag and release will be created by the script.
```
makeStopRelease.sh -b CMSSW8028_2016 -t CMSSW8028_2016_v1.0.0 -m "CMSSW8028_2016 config files. Weights are updated. The supplementaryFiles are included."
```


