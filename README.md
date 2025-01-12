r DISCONTINUATION OF PROJECT

This project will no longer be maintained by Intel.

Intel has ceased development and contributions including, but not limited to, maintenance, bug fixes, new releases, or updates, to this project.  

Intel no longer accepts patches to this project.

If you have an ongoing need to use this project, are interested in independently developing it, or would like to maintain patches for the open source software community, please create your own fork of this project.  

Contact: webadmin@linux.intel.com
Beignet
=======

Beignet is an open source implementation of the OpenCL specification - a generic
compute oriented API. This code base contains the code to run OpenCL programs on
Intel GPUs which basically defines and implements the OpenCL host functions
required to initialize the device, create the command queues, the kernels and
the programs and run them on the GPU. The code base also contains the compiler
part of the stack which is included in `backend/`. For more specific information
about the compiler, please refer to `backend/README.md`

News
----
[[Beignet project news|Beignet/NEWS]]

Prerequisite
------------

The project depends on the following external libraries:

- libdrm libraries (libdrm and libdrm\_intel)
- Various LLVM components
- If run with X server, beignet needs XLib, Xfixes and Xext installed. Otherwise, no X11 dependency.

And if you want to work with the standard ICD libOpenCL.so, then you need
two more packages (the following package name is for Ubuntu):

- ocl-icd-dev
- ocl-icd-libopencl1

If you don't want to enable ICD, or your system doesn't have ICD OpenCL support,
you must explicitly disable ICD support by running cmake with option `-DOCLICD_COMPAT=0`
then you can still link to the beignet OpenCL library. You can find the beignet/libcl.so
in your system's library installation directories.

Note that the compiler depends on LLVM (Low-Level Virtual Machine project), and the
project normally supports the 3 latest LLVM released versions.
Right now, the code has been compiled with LLVM 3.6, 3.7 and 3.8. With older
version LLVM from 3.3, build still support, but no full tests cover.

A simple command to install all the above dependencies for ubuntu or debian is:

`sudo apt-get install cmake pkg-config python ocl-icd-dev libegl1-mesa-dev`
`     ocl-icd-opencl-dev libdrm-dev libxfixes-dev libxext-dev llvm-3.6-dev`
`     clang-3.6 libclang-3.6-dev libtinfo-dev libedit-dev zlib1g-dev`

[http://llvm.org/releases/](http://llvm.org/releases/)


**The recommended LLVM/CLANG version is 3.6 and/or 3.7**

Based on our test result, LLVM 3.6 and 3.7 has the best pass rate on all the test suites. Compared
to LLVM 3.6 and 3.7, if you used LLVM 3.8, you should pay attention to float immediate. For example,
if you use 1.0 in the kernel, LLVM 3.6 will treat it as 1.0f, a single float, because the project
doesn't support double float. but LLVM 3.8 will treat it as 1.0, a double float, at the last it may cause
error. So we recommend using 1.0f instead of 1.0 if you don't need double float.

For LLVM 3.4 and 3.5, Beignet still support them, but it may be limited to support the
build and major functions.

How to build and install
------------------------

The project uses CMake with three profiles:

1. Debug (-g)
2. RelWithDebInfo (-g with optimizations)
3. Release (only optimizations)

Basically, from the root directory of the project

`> mkdir build`

`> cd build`

`> cmake ../ # to configure`

Please be noted that the code was compiled on GCC 4.6, GCC 4.7 and GCC 4.8 and CLANG 3.5 and
ICC 14.0.3. Since the code uses really recent C++11 features, you may expect problems with
older compilers. The default compiler should be GCC, and if you want to choose compiler manually,
you need to configure it as below:

`> cmake -DCOMPILER=[GCC|CLANG|ICC] ../`

CMake will check the dependencies and will complain if it does not find them.

`> make`

The cmake will build the backend firstly. Please refer to:
[[OpenCL Gen Backend|Beignet/Backend]] to get more dependencies.

Once built, the run-time produces a shared object libcl.so which basically
directly implements the OpenCL API.

`> make utest`

A set of tests are also produced. They may be found in `utests/`.

Simply invoke:

`> make install`

It installs the following six files to the beignet/ directory relatively to
your library installation directory.
- libcl.so
- libgbeinterp.so
- libgbe.so
- ocl\_stdlib.h, ocl\_stdlib.h.pch
- beignet.bc

It installs the OCL icd vendor files to /etc/OpenCL/vendors, if the system support ICD.
- intel-beignet.icd

`> make package`

It packages the driver binaries, you may copy&install the package to another machine with similar system.

How to run
----------

After building and installing Beignet, you may need to check whether it works on your
platform. Beignet also produces various tests to ensure the compiler and the run-time
consistency. This small test framework uses a simple c++ registration system to
register all the unit tests.

You need to call setenv.sh in the utests/ directory to set some environment variables
firstly as below:

`> . setenv.sh`

Then in `utests/`:

`> ./utest_run`

will run all the unit tests one after the others

`> ./utest_run some_unit_test`

will only run `some_unit_test` test.

On all supported target platform, the pass rate should be 100%. If it is not, you may
need to refer the "Known Issues" section. Please be noted, the `. setenv.sh` is only
required to run unit test cases. For all other OpenCL applications, don't execute that
command.

Normally, beignet needs to run under X server environment as normal user. If there isn't X server,
beignet provides two alternative to run:
* Run as root without X.
* Enable the drm render nodes by passing drm.rnodes=1 to the kernel boot args, then you can run beignet with non-root and without X.

Supported Targets
-----------------

 * 3rd Generation Intel Core Processors "Ivybridge".
 * 3rd Generation Intel Atom Processors "BayTrail".
 * 4th Generation Intel Core Processors "Haswell", need kernel patch if your linux kernel older than 4.2, see the "Known Issues" section.
 * 5th Generation Intel Core Processors "Broadwell".
 * 5th Generation Intel Atom Processors "Braswell".
 * 6th Generation Intel Core Processors "Skylake" and "Kabylake".
 * 5th Generation Intel Atom Processors "Broxten" or "Apollolake".

OpenCL 2.0
----------
From release v1.3.0, beignet supports OpenCL 2.0 on Skylake and later hardware.
This requires LLVM/Clang 3.9 or later, libdrm 2.4.66 or later and x86_64 linux.
As required by the OpenCL specification, kernels are compiled as OpenCL C 1.2 by default; to use 2.0 they
must explicitly request it with the -cl-std=CL2.0 build option.  As OpenCL 2.0 is likely to be slower than
1.2, we recommend that this is used only where needed.  (This is because 2.0 uses more registers and has
lots of int64 operations, and some of the 2.0 features (pipes and especially device queues) are implemented
in software so do not provide any performance gain.)  Beignet will continue to improve OpenCL 2.0 performance.

Known Issues
------------

* GPU hang issues.
  To check whether GPU hang, you could execute dmesg and check whether it has the following message:

  `[17909.175965] [drm:i915_hangcheck_hung] *ERROR* Hangcheck timer elapsed...`

  If it does, there was a GPU hang. Usually, this means something wrong in the kernel, as it indicates
  the OCL kernel hasn't finished for about 6 seconds or even more. If you think the OCL kernel does need
  to run that long and have confidence with the kernel, you could disable the linux kernel driver's
  hang check feature to fix this hang issue. Just invoke the following command on Ubuntu system:

  `# echo -n 0 > /sys/module/i915/parameters/enable_hangcheck`

  But this command is a little bit dangerous, as if your kernel really hangs, then the GPU will lock up
  forever until a reboot.

* "Beignet: self-test failed" and almost all unit tests fail.
  Linux 3.15 and 3.16 (commits [f0a346b](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=f0a346bdafaf6fc4a51df9ddf1548fd888f860d8)
  to [c9224fa](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=c9224faa59c3071ecfa2d4b24592f4eb61e57069))
  enable the register whitelist by default but miss some registers needed
  for Beignet.
  
  This can be fixed by upgrading Linux, or by disabling the whitelist:

  `# echo 0 > /sys/module/i915/parameters/enable_cmd_parser`

* "Beignet: self-test failed" and 15-30 unit tests fail on 4th Generation (Haswell) hardware.
  On Haswell, shared local memory (\_\_local) does not work at all on
  Linux <= 4.0, and requires the i915.enable_ppgtt=2 [boot parameter](https://wiki.ubuntu.com/Kernel/KernelBootParameters)
  on Linux 4.1.
  
  This is fixed in Linux 4.2; older versions can be fixed with
  [this patch](https://01.org/zh/beignet/downloads/linux-kernel-patch-hsw-support).
  
  If you do not need \_\_local, you can override the self-test with
  
  `export OCL_IGNORE_SELF_TEST=1`
  
  but using \_\_local after this may silently give wrong results.

* Precision issue.
  Currently Gen does not provide native support of high precision math functions
  required by OpenCL. We provide a software version to achieve high precision,
  which you can turn off through

  `# export OCL_STRICT_CONFORMANCE=0`.

  This loses some precision but gains performance.

* cl\_khr\_gl\_sharing.
  This extension is partially implemented(the most commonly used part), and we will implement
  other parts based on requirement.

Project repository
------------------
Right now, we host our project on fdo at:
[http://cgit.freedesktop.org/beignet/](http://cgit.freedesktop.org/beignet/).  
And the Intel 01.org:
[https://01.org/beignet](https://01.org/beignet)

The team
--------
Beignet project was created by Ben Segovia. Since 2013, Now Intel China OTC graphics
team continue to work on this project. The official contact for this project is:  
Zou Nanhai (<nanhai.zou@intel.com>).

Maintainers from Intel:

* Gong, Zhigang
* Yang, Rong

Developers from Intel:

* Song, Ruiling
* He, Junyan
* Luo, Xionghu
* Wen, Chuanbo
* Guo, Yejun
* Pan, Xiuli

Debian Maintainer:

* Rebecca Palmer

Fedora Maintainer:

* Igor Gnatenko

If I missed any other package maintainers, please feel free to contact the mail list.

How to contribute
-----------------
You are always welcome to contribute to this project, just need to subscribe
to the beignet mail list and send patches to it for review.
The official mail list is as below:
[http://lists.freedesktop.org/mailman/listinfo/beignet](http://lists.freedesktop.org/mailman/listinfo/beignet)  
The official bugzilla is at:
[https://bugs.freedesktop.org/enter_bug.cgi?product=Beignet](https://bugs.freedesktop.org/enter_bug.cgi?product=Beignet)  
You are welcome to submit beignet bug. Please be noted, please specify the exact platform
information, such as BYT/IVB/HSW/BDW, and GT1/GT2/GT3. You can easily get this information
by running the beignet's unit test.

Documents for OpenCL application developers
-------------------------------------------
- [[Cross compile (yocto)|Beignet/howto/cross-compiler-howto]]
- [[Work with old system without c++11|Beignet/howto/oldgcc-howto]]
- [[Kernel Optimization Guide|Beignet/optimization-guide]]
- [[Libva Buffer Sharing|Beignet/howto/libva-buffer-sharing-howto]]
- [[V4l2 Buffer Sharing|Beignet/howto/v4l2-buffer-sharing-howto]]
- [[OpenGL Buffer Sharing|Beignet/howto/gl-buffer-sharing-howto]]
- [[Video Motion Estimation|Beignet/howto/video-motion-estimation-howto]]
- [[Stand Alone Unit Test|Beignet/howto/stand-alone-utest-howto]]
- [[Android build|Beignet/howto/android-build-howto]]

The wiki URL is as below:
[http://www.freedesktop.org/wiki/Software/Beignet/](http://www.freedesktop.org/wiki/Software/Beignet/)
