
GOTO EndCommentLicense

 Written and contributed by Leonid Zadorin at the Centre for the Study of Choice
 (CenSoC), the University of Technology Sydney (UTS).

 Copyright (c) 2012 The University of Technology Sydney (UTS), Australia
 <www.uts.edu.au>

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 1. Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 3. All products which use or are derived from this software must display the
 following acknowledgement:
   This product includes software developed by the Centre for the Study of
   Choice (CenSoC) at The University of Technology Sydney (UTS) and its
   contributors.
 4. Neither the name of The University of Technology Sydney (UTS) nor the names
 of the Centre for the Study of Choice (CenSoC) and contributors may be used to
 endorse or promote products which use or are derived from this software without
 specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE CENTRE FOR THE STUDY OF CHOICE (CENSOC) AT THE
 UNIVERSITY OF TECHNOLOGY SYDNEY (UTS) AND CONTRIBUTORS ``AS IS'' AND ANY 
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 DISCLAIMED. IN NO EVENT SHALL THE CENTRE FOR THE STUDY OF CHOICE (CENSOC) OR
 THE UNIVERSITY OF TECHNOLOGY SYDNEY (UTS) OR CONTRIBUTORS BE LIABLE FOR ANY 
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

:EndCommentLicense

REM schtasks /create /tn censoc_nosleep /tr c:\censoc\nosleep\censoc_nosleep.exe /sc ONSTART /ru System 

REM this is a hack indeed -- later decouple between xp and 7 versions and produce a better power scheme/plan selection

powercfg /hibernate on

powercfg /n /change 0 /monitor-timeout-ac 25
powercfg /n /change 0 /disk-timeout-ac 50
powercfg /n /change 0 /standby-timeout-ac 0
powercfg /n /change 0 /hibernate-timeout-ac 380
powercfg /n /change 0 /processor-throttle-ac adaptive

powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e
powercfg -setabsentia 381b4222-f694-41f0-9685-ff5bb260df2e 

powercfg /change /monitor-timeout-ac 25
powercfg /change /disk-timeout-ac 50
powercfg /change /standby-timeout-ac 0
powercfg /change /hibernate-timeout-ac 380
powercfg /change /processor-throttle-ac adaptive

REM enable idle states on processor
powercfg -setacvalueindex 381b4222-f694-41f0-9685-ff5bb260df2e 54533251-82be-4824-96c1-47b60b740d00 5d76a2ca-e8c0-402f-a133-2158492d58ad 0
powercfg -setacvalueindex 381b4222-f694-41f0-9685-ff5bb260df2e 54533251-82be-4824-96c1-47b60b740d00 3b04d4fd-1cc7-4f23-ab1c-d1337819c4bb 1

REM processor power cstate and pstate (0,1=power saver, 2,3=balanced, 4,5=high perf)
powercfg -setacvalueindex 381b4222-f694-41f0-9685-ff5bb260df2e 54533251-82be-4824-96c1-47b60b740d00 68f262a7-f621-4069-b9a5-4874169be23c 3
powercfg -setacvalueindex 381b4222-f694-41f0-9685-ff5bb260df2e 54533251-82be-4824-96c1-47b60b740d00 bbdc3814-18e9-4463-8a55-d197327c45c0 4

REM minimum processor state
powercfg -setacvalueindex 381b4222-f694-41f0-9685-ff5bb260df2e 54533251-82be-4824-96c1-47b60b740d00 893dee8e-2bef-41e0-89c6-b55d0929964c 5

REM max processor state

powercfg -setacvalueindex 381b4222-f694-41f0-9685-ff5bb260df2e 54533251-82be-4824-96c1-47b60b740d00 bc5038f7-23e0-4960-96da-33abaf5935ec 100

REM processor power perfstate settings
powercfg -setacvalueindex 381b4222-f694-41f0-9685-ff5bb260df2e 54533251-82be-4824-96c1-47b60b740d00 bbdc3814-18e9-4463-8a55-d197327c45c0 3

REM PCI express link state
powercfg -setacvalueindex 381b4222-f694-41f0-9685-ff5bb260df2e 501a4d13-42af-4429-9fd1-a8218c268e20 ee12f906-d277-404b-b6da-e5fa1a576df5 0

REM password on wake up
powercfg -SETACVALUEINDEX 381b4222-f694-41f0-9685-ff5bb260df2e SUB_NONE CONSOLELOCK 1
powercfg -SETDCVALUEINDEX 381b4222-f694-41f0-9685-ff5bb260df2e SUB_NONE CONSOLELOCK 1

REM powercfg /setactive scheme_current
powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e
powercfg -setabsentia 381b4222-f694-41f0-9685-ff5bb260df2e 

diskperf -Y

