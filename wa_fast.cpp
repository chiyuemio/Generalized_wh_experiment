#include "wa.hpp"

//#include <ilcplex/ilocplex.h>
//ILOSTLBEGIN

#include <iostream>
#include <string>
#include <limits>
using namespace std;

/***
 * The weakly hard real-time analysis for periodic task systems.
 * Given 'K' successive activations of "ti", which is the maximum number of deadline misses exceeded 'm'?
 */
statWA wanalysis_fast(const Task& ti, const std::vector<Task>& hps, const int m, const int K) {

	cout << "Weakly hard real-time analysis of a task ti" << endl;
	double ci = ti.get_wcet(), di = ti.get_dline(), pi = ti.get_period(), Ri = ti.get_wcrt(), bri = ti.get_bcrt();
	double BP = ti.get_bp(); int Ni = ceil(BP / pi);
	double ui = ci / pi;
	double ji = ti.get_jitter();
	//double ji = 0;

	// the vector of wcet, dline, period and utilization of each higher priority task
	vector<double> c, d, p, u, br, jitt;
	for (unsigned int j = 1; j <= hps.size(); j++) {
		double cj = hps[j - 1].get_wcet(), dj = hps[j - 1].get_dline(), pj = hps[j - 1].get_period();
		double brj = hps[j - 1].get_bcrt();
		double jitj = hps[j - 1].get_jitter();
		//double jitj = 0;
		c.push_back(cj); d.push_back(dj); p.push_back(pj); u.push_back(cj / pj);
		br.push_back(brj);
		jitt.push_back(jitj);
	}

	const double M = 10000000, sigma1 = 1;

	vector<Task> copy_hps(hps);
	copy_hps.push_back(ti);
	vector<double> idle_lb; // minimum level-i idle time w.r.c time lengths of pi, 2*pi, 3*pi, ..., K*pi
	for (unsigned int i = 1; i <= K; i++) {
		idle_lb.push_back(computeIdle(copy_hps, i*pi));
	}

	IloEnv env;
	statWA swa;

	try {

		IloModel model(env);

		/**
		 * ASSUMPTIONS
		 *
		 * A1: the job in prior to the 1st job is schedulable (otherwise, we can always advance the job windows without reducing
		 *      the number of deadline misses)
		 *
		 * A2: the 1st job must misses its deadline, otherwise, we can always postpone the beginning of the problem windows without
		 *      reducing the number of deadline misses)
		 *
		 */


		 /*********************** VARIABLES **********************************/

		 // (4.1.1) Activation instant
		IloNumVarArray a(env, 0, 0, IloInfinity, ILOFLOAT);
		for (unsigned int k = 1; k <= K + 1; k++) {
			string name_a = "a" + convert_to_string(k);
			a.add(IloNumVar(env, 0, IloInfinity, ILOFLOAT, name_a.c_str()));
			if (k > 1) 
				model.add(IloRange(env, pi - ji, a[k - 1] - a[k - 2], pi + ji));
		}

		 // (4.1.2) Busy windows
		IloNumVarArray L(env, 0, 0, IloInfinity, ILOFLOAT);
		for (unsigned int k = 1; k <= K + 1; k++) {
			string name_bp = "bp" + convert_to_string(k);
			L.add(IloNumVar(env, 0, pi - bri + ji, ILOFLOAT, name_bp.c_str()));
		}
		// First activation
		model.add(IloRange(env, 0, a[0] - L[0], ji));

		// (4.1.3)a offsets 
		IloNumVarArray alpha(env, 0, 0, IloInfinity, ILOFLOAT);
		for (unsigned int j = 1; j <= hps.size(); j++) {
			string name_alpha = "alpha" + convert_to_string(j);
			alpha.add(IloNumVar(env, 0, p[j - 1] - br[j - 1] + jitt[j - 1], ILOFLOAT, name_alpha.c_str()));
		}


		//// (4.1.3)b check that at least one offset is 0
		IloNumVarArray b_off(env, 0, 0, 1, ILOBOOL);
		IloExpr sum_boff(env);
		for (unsigned int j = 1; j <= hps.size() + 1; j++) {
			string name_b_off = "b_off" + convert_to_string(j);
			b_off.add(IloNumVar(env, 0, 1, ILOBOOL, name_b_off.c_str()));
			sum_boff += b_off[j - 1];
			if (j <= hps.size())
				model.add(IloRange(env, -IloInfinity, alpha[j - 1] - M*b_off[j - 1], 0));
			else
				model.add(IloRange(env, -IloInfinity, L[0] - M*b_off[j - 1], 0));
		}
		model.add(IloRange(env, 0, sum_boff, hps.size()));

		// (4.1.4) idle times
		IloNumVarArray idle(env, 0, 0, IloInfinity, ILOFLOAT);
		for (unsigned int k = 1; k <= K; k++) {
			string name_idle = "idle" + convert_to_string(k);
			idle.add(IloNumVar(env, 0, pi - bri, ILOFLOAT, name_idle.c_str()));
		}

		// (4.1.5) finish times
		IloNumVarArray f(env, 0, 0, IloInfinity, ILOFLOAT);
		for (unsigned int k = 1; k <= K; k++) {
			string name_ft = "ft" + convert_to_string(k);
			f.add(IloNumVar(env, 0, IloInfinity, ILOFLOAT, name_ft.c_str()));

			model.add(IloRange(env, bri, f[k - 1] - a[k - 1], Ri));
			if (k > 1)
				model.add(IloRange(env, ci, f[k - 1] - f[k - 2], Ri + pi - bri + ji));
		}

		// (4.1.6) schedulability 
		// b == 1 --> the job is not schedulable; b == 0 --> schedulable
		IloNumVarArray b(env, 0, 0, 1, ILOBOOL);
		for (unsigned int k = 1; k <= K; k++) {
			string name = "b" + convert_to_string(k);
			b.add(IloNumVar(env, 0, 1, ILOBOOL, name.c_str()));

			IloExpr dk(env); dk += L[0] + (k - 1)*pi + di;
			model.add(IloRange(env, 0, M*b[k - 1] + dk - f[k - 1], IloInfinity));
			model.add(IloRange(env, -IloInfinity, dk - f[k - 1] - M*(1.0 - 1.0*b[k - 1]) + sigma1, 0));
		}
		// b[0] == 1: J_1 misses its deadline
		model.add(IloRange(env, 1, b[0], 1));


		// (4.1.7) interference from the previous job 
		IloNumVarArray bb(env, 0, 0, 1, ILOBOOL);
		for (unsigned int k = 1; k <= K; k++) {
			string name = "bb" + convert_to_string(k);
			bb.add(IloNumVar(env, 0, 1, ILOBOOL, name.c_str()));

			model.add(IloRange(env, 0, a[k] - f[k - 1] + M*bb[k - 1], IloInfinity));
			model.add(IloRange(env, -IloInfinity, a[k] - f[k - 1] + sigma1 - M*(1 - bb[k - 1]), 0));
			////idle = 0 --> bb == 1
			model.add(IloRange(env, -IloInfinity, idle[k - 1] - (1 - bb[k - 1])*M, 0));
		}


		// (4.1.8) counting jobs from a higher priority task
		//// nf_j_k: #jobs of tau_j in [0,f_k)
		//// nL_j_k: #jobs of tau_j in: [0, r_k-L_k) if b_{k-1}=0; [0,f_{k-1}) if  b_{k-1}=1
		std::vector<IloNumVarArray> nf, nL;
		for (unsigned int j = 1; j <= hps.size(); j++) {
			nf.push_back(IloNumVarArray(env, 0, 0, IloInfinity, ILOFLOAT));
			nL.push_back(IloNumVarArray(env, 0, 0, IloInfinity, ILOFLOAT));
			for (unsigned int k = 1; k <= K; k++) {
				string name_f = "nf_" + convert_to_string(j) + "_" + convert_to_string(k);
				nf[j - 1].add(IloNumVar(env, 0, IloInfinity, ILOFLOAT, name_f.c_str()));

				string name_b = "nL_" + convert_to_string(j) + "_" + convert_to_string(k);
				nL[j - 1].add(IloNumVar(env, 0, IloInfinity, ILOFLOAT, name_b.c_str()));
			}
			// nL_{K+1}
			string name_b = "nL_" + convert_to_string(j) + "_" + convert_to_string(K + 1);
			nL[j - 1].add(IloNumVar(env, 0, IloInfinity, ILOFLOAT, name_b.c_str()));
		}


		for (unsigned int j = 1; j <= hps.size(); j++) {
			for (unsigned int k = 1; k <= K; k++) {
				//// a coarse upper bound (uf) 
				model.add(IloRange(env, -IloInfinity, nf[j - 1][k - 1] - ceil((pi - bri + (k - 1)*pi + ji + Ri) / p[j - 1]), 0));

				//// nf 
				model.add(IloRange(env, 0, nf[j - 1][k - 1] * p[j - 1] - (f[k - 1] - alpha[j - 1] - jitt[j - 1]), IloInfinity));
				model.add(IloRange(env, -IloInfinity, (nf[j - 1][k - 1] - 1) * p[j - 1] + sigma1 - (f[k - 1] - alpha[j - 1] + jitt[j - 1]), 0));

				if (k > 1) {
					//// a coarse upper bound (uf) 
					model.add(IloRange(env, -IloInfinity, nL[j - 1][k - 1] - ceil((pi - bri + (k - 1)*pi + ji) / p[j - 1]) - bb[k - 2] * M, 0));

					//// nL 
					model.add(IloRange(env, 0, bb[k - 2] * M + nL[j - 1][k - 1] * p[j - 1] - (a[k - 1] - L[k - 1] - alpha[j - 1] - jitt[j - 1]), IloInfinity));
					model.add(IloRange(env, -IloInfinity, -bb[k - 2] * M + nL[j - 1][k - 1] * p[j - 1] + sigma1 - (a[k - 1] - L[k - 1] - alpha[j - 1] + jitt[j - 1]) - p[j - 1], 0));

				}
				else {
					model.add(IloRange(env, 0, nL[j - 1][k - 1], 0));
				}

			}
			// nL_{K+1}
			//// a coarse upper bound (uf) 
			model.add(IloRange(env, -IloInfinity, nL[j - 1][K] - ceil((pi - bri + K*pi + ji) / p[j - 1]) - bb[K - 1] * M, 0));

			//// nL [11]
			model.add(IloRange(env, 0, bb[K - 1] * M + nL[j - 1][K] * p[j - 1] - (a[K] - L[K] - alpha[j - 1] - jitt[j - 1]), IloInfinity));
			model.add(IloRange(env, -IloInfinity, -bb[K - 1] * M + nL[j - 1][K] * p[j - 1] + sigma1 - (a[K] - L[K] - alpha[j - 1] + jitt[j - 1]) - p[j - 1], 0));

		}

		//  nL_j_{k+1} = nk_j_k iff bb = 1
		for (unsigned int k = 2; k <= K + 1; k++) {
			for (unsigned int j = 1; j <= hps.size(); j++) {
				model.add(IloRange(env, 0, nL[j - 1][k - 1] - nf[j - 1][k - 2] + (1 - bb[k - 2])*M, IloInfinity));
				model.add(IloRange(env, -IloInfinity, nL[j - 1][k - 1] - nf[j - 1][k - 2] - (1 - bb[k - 2])*M, 0));
			}
		}


		// (4.1.9) refining the job counting for a higher priority task

		//// nfb_j_k[]
		vector< std::vector<IloNumVarArray> > nfb;
		for (unsigned int j = 1; j <= hps.size(); j++) {
			vector<IloNumVarArray> v;
			for (unsigned int k = 1; k <= K; k++) {
				IloNumVarArray vv(env, 0, 0, 1, ILOBOOL);
				for (unsigned int i = 1; i <= ceil((Ri + pi - bri + jitt[j - 1])/ p[j - 1]); i++) {
					string name = "nfb_" + convert_to_string(j) + "_" + convert_to_string(k) + "_" + convert_to_string(i);
					vv.add(IloNumVar(env, 0, 1, ILOBOOL, name.c_str()));
				}
				v.push_back(vv);
			}
			nfb.push_back(v);
		}
		////// nL_j_k + Delta = nf_j_k 
		for (unsigned int j = 1; j <= hps.size(); j++) {
			for (unsigned int k = 1; k <= K; k++) {
				IloExpr Delta(env);
				for (unsigned int i = 1; i <= ceil((Ri + pi - bri + jitt[j - 1]) / p[j - 1]); i++)
					Delta += nfb[j - 1][k - 1][i - 1];
				model.add(IloRange(env, 0, nL[j - 1][k - 1] + Delta - nf[j - 1][k - 1], 0));
			}
		}
		////// precedence constraint on ""nfb"s 
		for (unsigned int j = 1; j <= hps.size(); j++) {
			for (unsigned int k = 1; k <= K; k++) {
				for (unsigned int i = 1; i <= ceil((Ri + pi - bri + jitt[j - 1]) / p[j - 1]); i++) {
					if (i > 1)
						model.add(IloRange(env, 0, nfb[j - 1][k - 1][i - 1] - nfb[j - 1][k - 1][i - 2], 1));
				}
			}
		}

		//// nLb_j_k[]
		vector< std::vector<IloNumVarArray> > nLb;
		for (unsigned int j = 1; j <= hps.size(); j++) {
			vector<IloNumVarArray> v;
			for (unsigned int k = 1; k <= K + 1; k++) {
				IloNumVarArray vv(env, 0, 0, 1, ILOBOOL);
				for (unsigned int i = 1; i <= ceil((pi - bri + jitt[j - 1]) / p[j - 1]); i++) {
					string name = "nLb_" + convert_to_string(j) + "_" + convert_to_string(k) + "_" + convert_to_string(i);
					vv.add(IloNumVar(env, 0, 1, ILOBOOL, name.c_str()));
				}
				v.push_back(vv);
			}
			nLb.push_back(v);
		}
		////// if bb[k-2] == 0, nf[k-2]+Delta_p == nL[k-1] 
		for (unsigned int j = 1; j <= hps.size(); j++) {
			for (unsigned int k = 2; k <= K + 1; k++) {
				IloExpr Delta_p(env);
				for (unsigned int i = 1; i <= ceil((pi - bri + jitt[j - 1]) / p[j - 1]); i++)
					Delta_p += nLb[j - 1][k - 1][i - 1];
				model.add(IloRange(env, 0, nf[j - 1][k - 2] + Delta_p - nL[j - 1][k - 1] + bb[k - 2] * M, IloInfinity));
				model.add(IloRange(env, -IloInfinity, nf[j - 1][k - 2] + Delta_p - nL[j - 1][k - 1] - bb[k - 2] * M, 0));
			}
		}
		////// if bb[k-2] == 1, nLb == 0 
		for (unsigned int j = 1; j <= hps.size(); j++) {
			for (unsigned int k = 2; k <= K; k++) {
				for (unsigned int i = 1; i <= ceil((pi - bri + jitt[j - 1]) / p[j - 1]); i++) {
					model.add(IloRange(env, 0, nLb[j - 1][k - 1][i - 1] + (1 - bb[k - 2])*M, IloInfinity));
					model.add(IloRange(env, -IloInfinity, nLb[j - 1][k - 1][i - 1] - (1 - bb[k - 2])*M, 0));
				}
			}
		}
		////// nLb[j][k][i] >= nLb[j][k][i-1] 
		for (unsigned int j = 1; j <= hps.size(); j++) {
			for (unsigned int k = 1; k <= K + 1; k++) {
				for (unsigned int i = 1; i <= ceil((pi - bri + jitt[j - 1]) / p[j - 1]); i++) {
					if (i > 1)
						model.add(IloRange(env, 0, nLb[j - 1][k - 1][i - 1] - nLb[j - 1][k - 1][i - 2], 1));
				}
			}
		}
		////// the cumulative sum of boolean indications 
		for (unsigned int j = 1; j <= hps.size(); j++) {
			IloExpr totN(env);
			for (unsigned int k = 1; k <= K; k++) {

				if (k > 1) {
					for (unsigned int i = 1; i <= ceil((pi - bri + jitt[j - 1]) / p[j - 1]); i++)
						totN += nLb[j - 1][k - 1][i - 1];
				}

				for (unsigned int i = 1; i <= ceil((Ri + pi - bri + jitt[j - 1]) / p[j - 1]); i++)
					totN += nfb[j - 1][k - 1][i - 1];

				model.add(IloRange(env, 0, totN - nf[j - 1][k - 1], 0));

			}
		}

		////// nLb_j_1_p = 0
		for (unsigned int j = 1; j <= hps.size(); j++) {
			for (unsigned int i = 1; i <= ceil((pi - bri + jitt[j - 1]) / p[j - 1]); i++) {
				model.add(IloRange(env, 0, nLb[j - 1][0][i - 1], 0));
			}
		}

		/*********************** The end of declaration of VARIABLES **********************************/




		/*********************** CONSTRAINTS on schedulability analysis **********************************/

		// (4.2.1): idle time inside a job window
		for (unsigned int k = 2; k < K; k++) {
			IloExpr work(env);
			for (unsigned int j = 1; j <= hps.size(); j++) {
				work += (nL[j - 1][k - 1] - nf[j - 1][k - 2]) * c[j - 1];
			}
			model.add(IloRange(env, -IloInfinity, work + idle[k - 2] - (a[k - 1] - L[k - 1] - f[k - 2]) - bb[k - 2] * M, 0));
			model.add(IloRange(env, 0, work + idle[k - 2] - (a[k - 1] - L[k - 1] - f[k - 2]) + bb[k - 2] * M, IloInfinity));
		}

		// (4.2.2) "ft" by accumulating previous workload and idles
		for (unsigned int k = 1; k <= K; k++) {
			IloExpr totI(env);
			for (unsigned int j = 1; j <= hps.size(); j++) {
				totI += nf[j - 1][k - 1] * c[j - 1];
			}
			totI += k*ci;

			IloExpr totIdle(env);
			if (k > 1)
				for (unsigned int i = 1; i < k; i++) totIdle += idle[i - 1];

			model.add(IloRange(env, 0, totI + totIdle - f[k - 1], 0));
		}


		// (4.2.3) "ak-Lk" by accumulating previous workload and idles

		for (unsigned int k = 2; k <= K + 1; k++) {
			IloExpr totI(env);
			for (unsigned int j = 1; j <= hps.size(); j++) {
				totI += nL[j - 1][k - 1] * c[j - 1];
			}

			totI += (k - 1)*ci;

			IloExpr refer(env); refer = a[k - 1] - L[k - 1];

			IloExpr totIdle(env);
			for (unsigned int i = 1; i < k; i++) totIdle += idle[i - 1];

			model.add(IloRange(env, 0, totI + totIdle - refer + bb[k - 2] * M, IloInfinity));
			model.add(IloRange(env, -IloInfinity, totI + totIdle - refer - bb[k - 2] * M, 0));
		}


		// (4.2.4) Busy period when bb_{k-1} == 0
		for (unsigned int k = 1; k <= K; k++) {
			IloExpr totI(env);
			for (unsigned int j = 1; j <= hps.size(); j++) {
				totI += (nf[j - 1][k - 1] - nL[j - 1][k - 1]) * c[j - 1];
			}
			totI += ci;

			IloExpr refer(env); refer = a[k - 1] - L[k - 1];
			if (k == 1) {
				model.add(IloRange(env, -IloInfinity, totI - (f[k - 1] - refer), 0));
				model.add(IloRange(env, 0, totI - (f[k - 1] - refer), IloInfinity));
			}
			else {
				model.add(IloRange(env, -IloInfinity, totI - (f[k - 1] - refer) - M*(bb[k - 2]), 0));
				model.add(IloRange(env, 0, totI - (f[k - 1] - refer) + M*(bb[k - 2]), IloInfinity));
			}
		}

		// (4.2.5) Busy period when bb_{k-1} == 1
		for (unsigned int k = 2; k <= K; k++) {
			IloExpr totI(env);
			for (unsigned int j = 1; j <= hps.size(); j++) {
				totI += (nf[j - 1][k - 1] - nf[j - 1][k - 2]) * c[j - 1];
			}
			totI += ci;

			model.add(IloRange(env, -IloInfinity, totI - (f[k - 1] - f[k - 2]) - M*(1 - bb[k - 2]), 0));
			model.add(IloRange(env, 0, totI - (f[k - 1] - f[k - 2]) + M*(1 - bb[k - 2]), IloInfinity));

		}


		/************************** Additional constraints *************************/
		// C1: constraints on the minimum idle time/workload
		IloExpr idles(env);
		for (unsigned int k = 1; k <= K; k++) {
			idles += idle[k - 1];
			model.add(IloRange(env, idle_lb[k - 1], idles, IloInfinity));
		}

		for (unsigned int j = 1; j < K; j++) {
			// super coarse upper bound
			model.add(IloRange(env, -IloInfinity, idle[j - 1] - pi + ci, 0));
		}
		// C2: let's try to refine the beginning of "bp"
		for (unsigned int k = 2; k <= K + 1; k++) {
			for (unsigned int j = 1; j <= hps.size(); j++)
				model.add(IloRange(env, -IloInfinity, alpha[j - 1] + (nL[j - 1][k - 1] - 1)*p[j - 1] + br[j - 1] - (L[0] + (k - 1)*pi - L[k - 1]) - bb[k - 2] * M, 0));
		}
		// C3: let's try to refine the beginning of "ft"
		for (unsigned int k = 1; k <= K; k++) {
			for (unsigned int j = 1; j <= hps.size(); j++) {
				model.add(IloRange(env, -IloInfinity, alpha[j - 1] + (nf[j - 1][k - 1] - 1)*p[j - 1] + br[j - 1] + sigma1 - f[k - 1], 0));
			}
		}


		/*********************** Weakly hard real-time schedulability analysis **********************************/
		// (4.3) total number of deadline misses
		IloExpr nDmiss(env);
		for (unsigned int k = 1; k <= K; k++)
			nDmiss += b[k - 1];

		model.add(IloRange(env, m + 1, nDmiss, K));
		//model.add(IloRange(env, 0, nDmiss, K));
		model.add(IloMaximize(env, nDmiss));

		/********************************************************************************************************/

		//// Extracting the model
		IloCplex cplex(model);

		const int timeLimit = 60 * 60; 
		cplex.setParam(IloCplex::TiLim, timeLimit);

		cplex.setOut(env.getNullStream());

		double ss = cplex.getCplexTime();
		//cplex.exportModel("qcpex1.lp");

		// Set maximum number of threads 
		cplex.setParam(IloCplex::Threads, 40);

		// Stop at the first feasibile solution
		cplex.setParam(IloCplex::IntSolLim, 1);

		cplex.extract(model);
		bool fea = cplex.solve();



		////////////// Analyse data

		const IloAlgorithm::Status solStatus = cplex.getStatus();

		cout << "++++++++++++++++++++++++++++++++++++++++" << std::endl;
		cout << ">>>>>> " << solStatus << " <<<<<" << std::endl;
		cout << "++++++++++++++++++++++++++++++++++++++++" << std::endl;


		///////////////////////////////

		double ee = cplex.getCplexTime();
		bool bounded = false;

		swa.time = ee - ss;

		if (fea) {
			swa.bounded = true;
			swa.satisfies_mk = false;
			swa.m = cplex.getObjValue();
		}
		else {
			swa.m = cplex.getObjValue();
			if (swa.time < timeLimit) {
				swa.bounded = true;
				swa.satisfies_mk = true;
			}
			else {
				swa.bounded = false;
				swa.satisfies_mk = false;
			}
		}

		env.end();
		return swa;

	}
	catch (IloException& e) {
		cerr << "Concert exception caught: " << e << endl;
		return swa;
	}

	env.end();


}

