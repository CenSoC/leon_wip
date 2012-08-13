
# note -- no longer using explicit command-line. things like filename for certificate(s) -- those on some systems may be relevant w.r.t. CWD (current working dir) which may or may not be the same as the dir for the binary itself... yet the cfg/args may not always impose the absolute path (or indeed know of it) -- so even if starting from a batch file things like 'processor.exe' -- the batch/shell stcript may have an 'indeterminate' CWD on the target processing machin (from the stapoint of the script-generating server).
# CMDLINE="--cert certificate.pem --server_at 127.0.0.1:8080"
# CMDLINE="--cert certificate.pem --server_at localhost:8080"

# GDB this is for normal sh (not bash)
echo "catch throw" > processor.gdb
echo "run " >> processor.gdb
MALLOC_OPTIONS="10nPJ" gdb processor.exe -x processor.gdb
rm processor.gdb

exit

# for non-gdb runs
MALLOC_OPTIONS="10nPJ" ./processor.exe 

exit

# for release mode
MALLOC_OPTIONS="10nP" ./processor.exe

exit

# GDB this is for bash... unfinished (no catch throw cmd yet)
# need to put this at top #!/usr/local/bin/bash
MALLOC_OPTIONS="10nPJ" gdb processor.exe -x <(echo "run ") 

exit
