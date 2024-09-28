Regression Tests Guideline

Before performing regression tests, please make sure the compilation is successful. 

To perform individual regression test, run:
	./rt_xxx/run.sh

To perform all regression tests, run:
	./run_all.sh

To add new regression test, 
	1. Create new folder named rt_xxx.
	2. Follow rt_template by providing inputs, references, run script, and readme.txt of test specifications. 
	2. Edit ./run_all.sh script to include ./rt_xxx/run.sh script. 
