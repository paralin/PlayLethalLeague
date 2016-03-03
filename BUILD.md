To use this, here's a rough outline.

Install Anaconada, 32 bit. Run the commands:
 
 - `conda create -n py34 python=3.4 anaconda`
 - `activate py34`
 - `conda install mingw libpython`
 - `pip install --upgrade --no-deps git+git://github.com/Theano/Theano.git`
 - `pip install keras`

When running `offline_learning.py` be sure to call `activate py34` before.

To compile:

 - `mkdir build`
 - `cd build && cmake .. -DPYTHON_DIR=C:\Anaconda3\envs\py34\ -DBOOST_INCLUDE_DIR=C:\Boost\include\boost-160 -DBOOST_LIBRARY_DIR=C:\Boost\stage\lib`

etc...

You will need to compile boost python against conda.

Add to `project-config.jam`: `using python : 3.4 : "/Anaconda3/envs/py34/" : "/Anaconda3/envs/py34/include" : "/Anaconda3/envs/py34/libs" ;`

Then use:

`b2 link=shared runtime-link=shared --with-python stage`
