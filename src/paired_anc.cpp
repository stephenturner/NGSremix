#include <math.h>
#include <fstream>
#include <cmath>



void print_pars(double *par, int n){
  fprintf(stderr, "%f", par[0]);
  for (int i=1;i<n;i++)
    fprintf(stderr, " %f", par[i]);
  fprintf(stderr, "\n");
}

double loglike_paired(double **l, double *pars, int & nSites, int & nKs){
  double res=0;
  for(int i=0;i<nSites;i++){
    double temp = 0;
    for(int nk=0;nk<nKs;nk++)
      temp += (l[i][nk] * pars[nk]);
    res += log(temp);
  }
  return(res);
}

double prob_gl_anc_af(double *gl1, double & f1, double & f2){
  double p_g0fa = f1 *f2;
  double p_g1fa = f1 * (1-f2) + f2 * (1-f1);
  double p_g2fa = (1-f1) * (1-f2);
  // double p_g0fa = (1-f1) * (1-f2);
  // double p_g1fa = f1 * (1-f2) + f2 * (1-f1);
  // double p_g2fa = f1 * f2;
  return(gl1[0]*p_g0fa + gl1[1]*p_g1fa + gl1[2]*p_g2fa);
}

double prob_gt_anc_af(unsigned short int &gt1, double & f1, double & f2){

  double p_af;

  if(gt1==2){
    p_af = f1 * f2;
  }else if (gt1==1){
    p_af = (f1 * (1-f2) + f2 * (1-f1));
  }else if (gt1==0){
    p_af = (1-f1) * (1-f2);
  }else if(gt1==3){ // missing
    p_af = 0;
  }

  return p_af; // * gt1;
}


void em_anc_paired(double tolStop, int nSites, int nKs, double** pre_calc, double *res2, int &numIter, int nIter){
  int useSq = 1;

  int stepMax = 1;
  int mstep = 4;

  double temppart[nKs];
  double x[nKs];
  double p0[nKs];
  double p1[nKs];
  double q1[nKs];
  double q2[nKs];

  double sr2;
  double sq2;
  double sv2;
  double ttol=0.0000001;
  double rowsum = 0;
  double norma;
  double alpha;


  for(int a=0; a<nKs; a++)
    x[a] = 1.0 / nKs;

  for(int iter=0;iter<nIter;iter++){
    numIter=iter;

    double pars[nKs];
    for(int a=0; a<nKs; a++)
      pars[a] = 0;
    for(int i=0;i<nSites;i++){

      for(int a=0; a<nKs; a++)
        temppart[a] = 0;

      rowsum = 0;

      for(int nk=0;nk<nKs;nk++){
        temppart[nk] = (pre_calc[i][nk] * x[nk]);
        rowsum += temppart[nk];
      }
      for(int nk=0;nk<nKs;nk++)
        pars[nk] += temppart[nk] / rowsum;

    }
    double total = 0;
    for(int nk=0;nk<nKs;nk++)
      total += pars[nk];

    // normalize
    for(int nk=0;nk<nKs;nk++)
      x[nk] = pars[nk] / total;


    // new estimate is now done.

    // if(iter%10==0){
    //   double ll = loglike_paired(pre_calc, x, nSites, nKs);
    //   fprintf(stderr, "%d log: %f ", iter, ll);
    //   print_pars(x, nKs);
    //   fprintf(stderr, "\n");
    // }


    if(useSq && iter%3==2){

      sr2=0;
      sq2=0;
      sv2=0;

      //get stop sizes
      for(int j=0;j<nKs;j++){
        q1[j] = p1[j] - p0[j];
        sr2+= q1[j]*q1[j];
        q2[j] = x[j] - p1[j];
        sq2+= q2[j]*q2[j];
        sv2+= (q2[j]-q1[j])*(q2[j]-q1[j]);
      }

      //Stop the algorithm if the step size less than tolStop
      bool conv_bool = false;
      for (int j=0;j<nKs;j++){
        if((p1[j]>0.999 && q2[j]>0)){
          conv_bool=true;
          break;
        }
      }


      if(sqrt(sr2)<tolStop || sqrt(sq2)<tolStop || conv_bool){
        tolStop=sr2;
        break;
      }

      //calc alpha and map into [1,stepMax] if needed
      alpha = sqrt(sr2/sv2);
      if(alpha>stepMax)
        alpha=stepMax;
      if(alpha<1)
        alpha=1;

      //the magical step
      for(int j=0;j<nKs;j++)
        x[j] = p0[j] + 2 * alpha * q1[j] + alpha*alpha * (q2[j] - q1[j]);

      //in the rare instans that the boundarys are crossed. map into [ttol,1-ttol]
      for(int j=0;j<nKs;j++){
        if(x[j]<ttol)
          x[j]=ttol;
        if(x[j]>1-ttol)
          x[j]=1-ttol;

      }

      norma = 0;
      for(int j=0;j<nKs;j++)
        norma += x[j];

      for(int j=0;j<nKs;j++)
        x[j] /= norma;

      //change step size
      if (alpha == stepMax)
        stepMax = mstep * stepMax;
    }



    if(useSq && iter%3==0){
      for(int j=0;j<nKs;j++)
        p0[j] = x[j];
    }
    if(useSq && iter%3==1){
      for(int j=0;j<nKs;j++)
        p1[j] = x[j];
    }
  }

  for(int j=0;j<nKs;j++)
    res2[j] = x[j];
}

int is_missing2(double *ary){
  if(fabs(ary[0] - ary[1])<1e-6 && fabs(ary[0] - ary[2])<1e-6 && fabs(ary[1] - ary[2])<1e-6)
    return 1;
  else
    return 0;
}


int est_paired_anc_gl(int nSites, int K, int nKs, double *gl1, double **f, double *res2){

  int totsites = 0;
  int * keeplist  = new int[nSites];
  for(int i=0;i<nSites;i++){
    if(is_missing2(&gl1[i*3])){
      keeplist[i] = 0;
    } else{
      keeplist[i] = 1;
      totsites++;
    }
  }

  int npop = K;
  double** pre_calc = new double*[totsites];
  int totsites_idx = 0;
  for(int i=0;i<nSites;i++){
    if(keeplist[i]==0)
      continue;
    pre_calc[totsites_idx] = new double[nKs];
    int idx = 0;
    for(int a11=0;a11<npop;a11++){
      for(int a12=a11;a12<npop;a12++){
        pre_calc[totsites_idx][idx] = prob_gl_anc_af(&gl1[i*3], f[i][a11], f[i][a12]);
        idx++;
      }
    }
    totsites_idx++;
  }

  int maxIter = 5000;
  int currIter = 0;
  double tolStop=0.000001;
  em_anc_paired(tolStop, totsites, nKs, pre_calc, res2, currIter, maxIter);
  // double ll = loglike_paired(pre_calc, res2, nSites, nKs);
  // fprintf(stderr, "final log (iter: %d): %f ", currIter, ll);
  // print_pars(res2, nKs);
  // fprintf(stderr, "\n");

  // clean up
  for(int i=0;i<totsites;i++)
    delete[] pre_calc[i];
  delete[] pre_calc;
  delete[] keeplist;
  return currIter;
}


int est_paired_anc_gt(int nSites, int K, int nKs, unsigned short int *gt1, double **f, double *res2){

  int totsites = 0;
  int * keeplist  = new int[nSites];
  for(int i=0;i<nSites;i++){
    if(gt1[i]==3){
      keeplist[i] = 0;
    } else{
      keeplist[i] = 1;
      totsites++;
    }
  }

  int npop = K;
  double** pre_calc = new double*[totsites];
  int totsites_idx = 0;
  for(int i=0;i<nSites;i++){
    if(keeplist[i]==0)
      continue;
    pre_calc[totsites_idx] = new double[nKs];
    int idx = 0;
    for(int a11=0; a11<npop; a11++){
      for(int a12=a11; a12<npop; a12++){
        pre_calc[totsites_idx][idx] = prob_gt_anc_af(gt1[i], f[i][a11], f[i][a12]);
        idx++;
      }
    }
    totsites_idx++;
  }

  int maxIter = 5000;
  int currIter = 0;
  double tolStop=0.000001;
  em_anc_paired(tolStop, totsites, nKs, pre_calc, res2, currIter, maxIter);
  // double ll = loglike_paired(pre_calc, res2, nSites, nKs);
  // fprintf(stderr, "final log (iter: %d): %f ", currIter, ll);
  // print_pars(res2, nKs);
  // fprintf(stderr, "\n");




  // clean up
  for(int i=0;i<totsites;i++)
    delete[] pre_calc[i];
  delete[] pre_calc;
  delete[] keeplist;
  return currIter;
}
