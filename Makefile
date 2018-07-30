all:
	(cd fmu10; $(MAKE))
	(cd fmu20; $(MAKE))

clean:
	(cd fmu10; $(MAKE) clean)
	(cd fmu20; $(MAKE) clean)

distclean: clean
	rm -f fmu10/bin/fmusim_cs* fmu10/bin/fmusim_me*
	rm -f fmu20/bin/fmusim_cs* fmu20/bin/fmusim_me*
	rm -rf fmu10/fmu
	rm -rf fmu20/fmu
	rm -rf fmuTmp* 
	rm -f *.csv
	find . -name "*~" -exec rm {} \;
	find . -name "#*~" -exec rm {} \;

run_all: run_all_fmu10 run_all_fmu20

# Run all the fmu10 fmus.  Args are from run_all.bat
run_all_fmu10:
	fmu10/bin/fmusim_me fmu10/fmu/me/bouncingBall.fmu 4 0.01 0 c
	mv result.csv result_me10_bouncingBall.csv
	fmu10/bin/fmusim_cs fmu10/fmu/cs/bouncingBall.fmu 4 0.01 0 c
	mv result.csv result_cs10_bouncingBall.csv
	#
	fmu10/bin/fmusim_me fmu10/fmu/me/vanDerPol.fmu 5 0.1 0 c
	mv result.csv result_me10_vanDerPol.csv
	fmu10/bin/fmusim_cs fmu10/fmu/cs/vanDerPol.fmu 5 0.1 0 c
	mv result.csv result_cs10_vanDerPol.csv
	#
	fmu10/bin/fmusim_me fmu10/fmu/me/dq.fmu 1 0.1 0 c
	mv result.csv result_me10_dq.csv
	fmu10/bin/fmusim_cs fmu10/fmu/cs/dq.fmu 1 0.1 0 c
	mv result.csv result_cs10_dq.csv
	#
	fmu10/bin/fmusim_me fmu10/fmu/me/inc.fmu 15 0.1 0 c
	mv result.csv result_me10_inc.csv
	fmu10/bin/fmusim_cs fmu10/fmu/cs/inc.fmu 15 0.1 0 c
	mv result.csv result_cs10_inc.csv
	#
	fmu10/bin/fmusim_me fmu10/fmu/me/values.fmu 12 0.1 0 c
	mv result.csv result_me10_values.csv
	fmu10/bin/fmusim_cs fmu10/fmu/cs/values.fmu 12 0.1 0 c
	mv result.csv result_cs10_values.csv

# Run all the fmu20 fmus.  Args are from run_all.bat
run_all_fmu20:
	fmu20/bin/fmusim_me fmu20/fmu/me/bouncingBall.fmu 4 0.01 0 c
	mv result.csv result_me20_bouncingBall.csv
	fmu20/bin/fmusim_cs fmu20/fmu/cs/bouncingBall.fmu 4 0.01 0 c
	mv result.csv result_cs20_bouncingBall.csv
	#
	fmu20/bin/fmusim_me fmu20/fmu/me/vanDerPol.fmu 5 0.1 0 c
	mv result.csv result_me20_vanDerPol.csv
	fmu20/bin/fmusim_cs fmu20/fmu/cs/vanDerPol.fmu 5 0.1 0 c
	mv result.csv result_cs20_vanDerPol.csv
	#
	fmu20/bin/fmusim_me fmu20/fmu/me/dq.fmu 1 0.1 0 c
	mv result.csv result_me20_dq.csv
	fmu20/bin/fmusim_cs fmu20/fmu/cs/dq.fmu 1 0.1 0 c
	mv result.csv result_cs20_dq.csv
	#
	fmu20/bin/fmusim_me fmu20/fmu/me/inc.fmu 15 0.1 0 c
	mv result.csv result_me20_inc.csv
	fmu20/bin/fmusim_cs fmu20/fmu/cs/inc.fmu 15 0.1 0 c
	mv result.csv result_cs20_inc.csv
	#
	fmu20/bin/fmusim_me fmu20/fmu/me/values.fmu 12 0.1 0 c
	mv result.csv result_me20_values.csv
	fmu20/bin/fmusim_cs fmu20/fmu/cs/values.fmu 12 0.1 0 c
	mv result.csv result_cs20_values.csv
