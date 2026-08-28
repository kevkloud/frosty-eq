# CMake generated Testfile for 
# Source directory: /Users/kloud/Developer/frosty-eq
# Build directory: /Users/kloud/Developer/frosty-eq/build-release
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test("parameters" "/Users/kloud/Developer/frosty-eq/build-release/FrostyEQTests")
set_tests_properties("parameters" PROPERTIES  _BACKTRACE_TRIPLES "/Users/kloud/Developer/frosty-eq/CMakeLists.txt;79;add_test;/Users/kloud/Developer/frosty-eq/CMakeLists.txt;0;")
add_test("dsp" "/Users/kloud/Developer/frosty-eq/build-release/FrostyDspTests")
set_tests_properties("dsp" PROPERTIES  _BACKTRACE_TRIPLES "/Users/kloud/Developer/frosty-eq/CMakeLists.txt;93;add_test;/Users/kloud/Developer/frosty-eq/CMakeLists.txt;0;")
subdirs("libs/JUCE")
