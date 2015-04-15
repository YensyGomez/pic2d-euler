#include <iostream>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <cmath>
#include <complex>
#include <ctime>
#include <cstring>
#include <fstream>
#include <complex.h>
#include <fftw3.h>

//Magnitudes f.

#define m_e         9.10938291e-31	// Masa del Electrón
#define e           1.6021e-19		  // Carga del Electrón
#define k_b         1.3806504e-23	  // Constante de Boltzmann
#define epsilon_0	  8.854187e-12	  // Permitividad eléctrica del vacío

#define max_SPe     100000        	// Limite (computacional) de Superpartículas electrónicas
#define max_SPi     100000      	  // Limite (computacional) de Superpartículas iónicas
#define J_X         4095      		  // Número de puntos de malla X. Recomendado: Del orden 2^n-1
#define J_Y			    64			          // Número de puntos de malla Y. Recomendado: Del orden 2^n
//#define nptp        1388     	      // Número de pasos de tiempo por periodo plásmico electrónico (1388) 

using namespace std;

double RANDMAX;

void	  Cond_inic(void);
double	distrib_vel_X(double fmax, double vphi);
double	distrib_vel_Y(double fmax, double vphi);
double distrib_vel_Y_temp(double fmax,double vphi, int distemp);

void	  Concentration(double pos[max_SPe][2], double n[J_X][J_Y], int NSP);

void 	  Poisson2D_DirichletX_PeriodicY(double phi[J_X][J_Y],complex <double> rho[J_X][J_Y]);
void    Poisson2D_DirichletX_DirichletY(double phi[J_X][J_Y],complex <double> rho[J_X][J_Y]);
void	  Electric_Field (double phi[J_X][J_Y], double E_X[J_X][J_Y], double E_Y[J_X][J_Y]);

void 	  Motion(double pos[max_SPe][2],  double vel[max_SPe][2],  int NSP, int especie);
void    Funcion_Distribucion(double pos[max_SPe][2], double vel[max_SPe][2] , int NSP, char *archivo_X, char *archivo_Y);

double  pos_e[max_SPe][2],vel_e[max_SPe][2],pos_i[max_SPi][2], vel_i[max_SPi][2]; //Posiciones y Velocidades
double	ne[J_X][J_Y],ni[J_X][J_Y];     //Densidad de Partículas
complex <double> rho[J_X][J_Y];
double phi[J_X][J_Y], E_X[J_X][J_Y], E_Y[J_X][J_Y];

int     electrones=0, Iones=1;
int     X=0, Y=1;

//************************
// Parámetros del sistema
//************************

double	razon_masas=1.98e5; 		// m_i/m_e (Plata)
double	m_i=razon_masas*m_e;		// masa Ión
double	flujo_inicial=4.5e33;		// Flujo inicial (# part/m^2*s)
double	vflux_i[2]={1.e3,0.5e2};		// Velocidad de flujo iónico (m/s) = sqrt(2*k_b*Te/(M_PI*m_i))
double	vflux_i_magnitud=sqrt(vflux_i[0]*vflux_i[0]+vflux_i[1]*vflux_i[1]);
double	vflux_e[2]={sqrt(razon_masas)*vflux_i[0],sqrt(razon_masas)*vflux_i[1]};
double	vflux_e_magnitud=sqrt(vflux_e[0]*vflux_e[0]+vflux_e[1]*vflux_e[1]);
double	ni03D=flujo_inicial/vflux_i_magnitud;	// Concentración inicial de iones (3D)
double	ne03D=flujo_inicial/vflux_e_magnitud;	// Concentración inicial de electrones (3D)
double	Te=M_PI*0.5*m_e*pow(vflux_e_magnitud,2)/k_b;		// Temperatura electrónica inicial (°K)                    
                                                // (vflujo=sqrt(2k_bTe/(pi*me))

double	lambda_D=sqrt(epsilon_0*k_b*Te/(ne03D*pow(e,2)));  //Longitud de Debye
double	om_p=vflux_e_magnitud/lambda_D;                    //Frecuencia del plasma
double	ND=ne03D*pow(lambda_D,3);                          //Parámetro del plasma
int 	  NTe=1e5, NTI=1e5;                                  //Número de partículas "reales"



//***************************
//Constantes de normalización
//***************************

double	t0=1e-13;		          //Escala de tiempo: Tiempo de vaporización
//double	x0=vflux_i[0]*t0;   //Escala de longitud: Distancia recorrida en x por un ión en el tiempo t_0
double  x0=lambda_D;
double  n0=double(NTe)/(x0*x0*x0);


//************************
//Parámetros de simulación
//************************

double  delta_X=lambda_D;   //Paso espacial
double  Lmax[2]={(J_X-1)*delta_X, (J_Y-1)*delta_X}; //Tamaño del espacio de simulación.
int     Factor_carga_e=10, Factor_carga_i=10;       //Número de partículas por superpartícula.
int 	  k_max_inj;   //Tiempo máximo de inyección
int 	  K_total;     //Tiempo total
int 	  Ntv=6;
int 	  le=0, li=0,kt;
int 	  NTSPe, NTSPI, max_SPe_dt, max_SPi_dt;

double  L_max[2],  T,dt,t_0, ne0_3D,ni0_3D,ni0_1D,ne0_1D, vphi_i[2],vphi_e[2], vphi_i_magnitud, vphi_e_magnitud ,fi_Maxwell[2],fe_Maxwell[2],vpart,x_0,phi_inic;
double  Np_t, ni0_cell_1;

double  cte_rho=pow(e*t0,2)/(m_i*epsilon_0*pow(x0,3)); //Normalización de epsilon_0
double  phi0=2.*k_b*Te/(M_PI*e), E0=phi0/x0;
double  cte_E=t0*e*E0/(vflux_i[0]*m_e),fact_el=-1, fact_i=1./razon_masas;
//double  cte_E=x0*m_i/(e*t0),fact_el=-1, fact_i=1./razon_masas;
double  Ek_i,Ek_e,E_field,E_total, E_perdida;
double  Et0=k_b*Te;


double  vxe_perd[max_SPe];
int     perdidas[max_SPe];  
int     perd=0;
int     total_e_perdidos=0;
int     total_i_perdidos=0;
double  mv2perdidas=0;
FILE    *outPot19,*outEnergia, *outPot0_6, *outPot0_9, *outPot1_5, *outPot3_5, *outPot5_5, *outPot15;

FILE    *outFase_ele[61];
FILE    *outFase_ion[61];

double hx;


int main()
{

  int seed = time (NULL); srand (seed);  // Semilla para generar números aleatorios dependiendo del reloj interno.

  //******************
  //ARCHIVOS DE SALIDA
  //******************
  outEnergia=fopen("Energia","w");
  outPot0_6=fopen("potencial0-6","w");
  outPot0_9=fopen("potencial0-9","w");
  outPot1_5=fopen("potencial1-5","w");
  outPot3_5=fopen("potencial3-5","w");
  outPot5_5=fopen("potencial5-5","w");
  outPot15=fopen("potencial15","w");
  outPot19=fopen("potencial19","w");

  outFase_ele[0]=fopen("fase_ele0","w");outFase_ele[1]=fopen("fase_ele1","w");outFase_ele[2]=fopen("fase_ele2","w");outFase_ele[3]=fopen("fase_ele3","w");outFase_ele[4]=fopen("fase_ele4","w");
  outFase_ele[5]=fopen("fase_ele5","w");outFase_ele[6]=fopen("fase_ele6","w");outFase_ele[7]=fopen("fase_ele7","w");outFase_ele[8]=fopen("fase_ele8","w");outFase_ele[9]=fopen("fase_ele9","w");
  outFase_ele[10]=fopen("fase_ele10","w");outFase_ele[11]=fopen("fase_ele11","w");outFase_ele[12]=fopen("fase_ele12","w");outFase_ele[13]=fopen("fase_ele13","w");outFase_ele[14]=fopen("fase_ele14","w");
  outFase_ele[15]=fopen("fase_ele15","w");outFase_ele[16]=fopen("fase_ele16","w");outFase_ele[17]=fopen("fase_ele17","w");outFase_ele[18]=fopen("fase_ele18","w");outFase_ele[19]=fopen("fase_ele19","w");
  outFase_ele[20]=fopen("fase_ele20","w");outFase_ele[21]=fopen("fase_ele21","w");outFase_ele[22]=fopen("fase_ele22","w");outFase_ele[23]=fopen("fase_ele23","w");outFase_ele[24]=fopen("fase_ele24","w");
  outFase_ele[25]=fopen("fase_ele25","w");outFase_ele[26]=fopen("fase_ele26","w");outFase_ele[27]=fopen("fase_ele27","w");outFase_ele[28]=fopen("fase_ele28","w");outFase_ele[29]=fopen("fase_ele29","w");
  outFase_ele[30]=fopen("fase_ele30","w");outFase_ele[31]=fopen("fase_ele31","w");outFase_ele[32]=fopen("fase_ele32","w");outFase_ele[33]=fopen("fase_ele33","w");outFase_ele[34]=fopen("fase_ele34","w");
  outFase_ele[35]=fopen("fase_ele35","w");outFase_ele[36]=fopen("fase_ele36","w");outFase_ele[37]=fopen("fase_ele37","w");outFase_ele[38]=fopen("fase_ele38","w");outFase_ele[39]=fopen("fase_ele39","w");
  outFase_ele[40]=fopen("fase_ele40","w");outFase_ele[41]=fopen("fase_ele41","w");outFase_ele[42]=fopen("fase_ele42","w");outFase_ele[43]=fopen("fase_ele43","w");outFase_ele[44]=fopen("fase_ele44","w");
  outFase_ele[45]=fopen("fase_ele45","w");outFase_ele[46]=fopen("fase_ele46","w");outFase_ele[47]=fopen("fase_ele47","w");outFase_ele[48]=fopen("fase_ele48","w");outFase_ele[49]=fopen("fase_ele49","w");
  outFase_ele[50]=fopen("fase_ele50","w");outFase_ele[51]=fopen("fase_ele51","w");outFase_ele[52]=fopen("fase_ele52","w");outFase_ele[53]=fopen("fase_ele53","w");outFase_ele[54]=fopen("fase_ele54","w");
  outFase_ele[55]=fopen("fase_ele55","w");outFase_ele[56]=fopen("fase_ele56","w");outFase_ele[57]=fopen("fase_ele57","w");outFase_ele[58]=fopen("fase_ele58","w");outFase_ele[59]=fopen("fase_ele59","w");
  outFase_ele[60]=fopen("fase_ele60","w");
  

  outFase_ion[0]=fopen("fase_ion0","w");outFase_ion[1]=fopen("fase_ion1","w");outFase_ion[2]=fopen("fase_ion2","w");outFase_ion[3]=fopen("fase_ion3","w");outFase_ion[4]=fopen("fase_ion4","w");
  outFase_ion[5]=fopen("fase_ion5","w");outFase_ion[6]=fopen("fase_ion6","w");outFase_ion[7]=fopen("fase_ion7","w");outFase_ion[8]=fopen("fase_ion8","w");outFase_ion[9]=fopen("fase_ion9","w");
  outFase_ion[10]=fopen("fase_ion10","w");outFase_ion[11]=fopen("fase_ion11","w");outFase_ion[12]=fopen("fase_ion12","w");outFase_ion[13]=fopen("fase_ion13","w");outFase_ion[14]=fopen("fase_ion14","w");
  outFase_ion[15]=fopen("fase_ion15","w");outFase_ion[16]=fopen("fase_ion16","w");outFase_ion[17]=fopen("fase_ion17","w");outFase_ion[18]=fopen("fase_ion18","w");outFase_ion[19]=fopen("fase_ion19","w");
  outFase_ion[20]=fopen("fase_ion20","w");outFase_ion[21]=fopen("fase_ion21","w");outFase_ion[22]=fopen("fase_ion22","w");outFase_ion[23]=fopen("fase_ion23","w");outFase_ion[24]=fopen("fase_ion24","w");
  outFase_ion[25]=fopen("fase_ion25","w");outFase_ion[26]=fopen("fase_ion26","w");outFase_ion[27]=fopen("fase_ion27","w");outFase_ion[28]=fopen("fase_ion28","w");outFase_ion[29]=fopen("fase_ion29","w");
  outFase_ion[30]=fopen("fase_ion30","w");outFase_ion[31]=fopen("fase_ion31","w");outFase_ion[32]=fopen("fase_ion32","w");outFase_ion[33]=fopen("fase_ion33","w");outFase_ion[34]=fopen("fase_ion34","w");
  outFase_ion[35]=fopen("fase_ion35","w");outFase_ion[36]=fopen("fase_ion36","w");outFase_ion[37]=fopen("fase_ion37","w");outFase_ion[38]=fopen("fase_ion38","w");outFase_ion[39]=fopen("fase_ion39","w");
  outFase_ion[40]=fopen("fase_ion40","w");outFase_ion[41]=fopen("fase_ion41","w");outFase_ion[42]=fopen("fase_ion42","w");outFase_ion[43]=fopen("fase_ion43","w");outFase_ion[44]=fopen("fase_ion44","w");
  outFase_ion[45]=fopen("fase_ion45","w");outFase_ion[46]=fopen("fase_ion46","w");outFase_ion[47]=fopen("fase_ion47","w");outFase_ion[48]=fopen("fase_ion48","w");outFase_ion[49]=fopen("fase_ion49","w");
  outFase_ion[50]=fopen("fase_ion50","w");outFase_ion[51]=fopen("fase_ion51","w");outFase_ion[52]=fopen("fase_ion52","w");outFase_ion[53]=fopen("fase_ion53","w");outFase_ion[54]=fopen("fase_ion54","w");
  outFase_ion[55]=fopen("fase_ion55","w");outFase_ion[56]=fopen("fase_ion56","w");outFase_ion[57]=fopen("fase_ion57","w");outFase_ion[58]=fopen("fase_ion58","w");outFase_ion[59]=fopen("fase_ion59","w");
  outFase_ion[60]=fopen("fase_ion60","w");

  printf("ni03D=%e \nne03D=%e \nTemperatura electronica=%e \nLongitud de Debye=%e  \nFrecuencia del plasma=%e \nTp=%e \nND=%e \nLX=%e \nLY=%e \n",ni03D,ne03D,Te,lambda_D,om_p,2*M_PI/om_p,ND,Lmax[0],Lmax[1]);

  printf("cte_E=%e  \ncte_rho=%e  \nTe = %e  \nhx*Ld = %e  \n",cte_E,cte_rho, Te, hx*lambda_D );

  //printf("dt/t0=%e    \ndt/T=%e   \nhx/lambda_D=%e \nTiempo vapor.=%d dt \n",dt/t_0,dt/T,hx/(lambda_D/x0), k_max_inj);

  //***************************
  // Normalización de variables
  //***************************

  L_max[0]=Lmax[0]/x0;                      // Longitud región de simulación
  L_max[1]=Lmax[1]/x0;                      // Longitud región de simulación
  hx=delta_X/x0;
  //T=Periodo_pe/t0;                       // Periodo pionico normalizado resp.
  //dt=T/(nptp-1);                         // Paso temporal
  //dt=0.05*(1/om_p)/t0;
  t_0=1.;                             // tiempo de vaporación
  double n_0=n0*x0*x0*x0;             
  dt=1.e-5;
  ni0_3D=ni03D*pow(x0,3);                // Concentración de iones inicial 3D 
  ne0_3D=ne03D*pow(x0,3);                // Concentración de electrones inicial 3D
  vphi_i[0]=vflux_i[0]/vflux_i[0];
  vphi_e[0]=vflux_e[0]/vflux_i[0];
  vphi_i[1]=vflux_i[1]/vflux_i[0];
  vphi_e[1]=vflux_e[1]/vflux_i[0];
  fi_Maxwell[0]=  (2./(M_PI*vphi_i[0])); // Valor Máximo de la función de distribución Semi-Maxwelliana Iónica
  fe_Maxwell[0]=  (2./(M_PI*vphi_e[0])); // Valor Máximo de la función de distribución Semi-Maxwelliana electrónica
  fi_Maxwell[1]=  (1./(M_PI*vphi_i[1])); // Valor Máximo de la función de distribución Semi-Maxwelliana Iónica
  fe_Maxwell[1]=  (1./(M_PI*vphi_e[1])); // Valor Máximo de la función de distribución Semi-Maxwelliana electrónica
  //x_0=vphi_i[0]*t_0;
  NTSPe=NTe/Factor_carga_e, NTSPI=NTI/Factor_carga_i; // Número total de superpartículas
                                                      // Inyectadas en un tiempo t0.
                                                      // (= número de superpartículas
                                                      // Inyectadas por unidad de tiempo,  
                                                      // puesto que t0*(normalizado)=1.

  //double Ef0=(phi0*n0);

  //double Ek0_i=0.5*(pow(vphi_i_magnitud,2)); //energia cinetica ionica inicial
  //double Ek0_e=0.5*(pow(vphi_e_magnitud,2)); //energia cinetica electronica inicial
  double E_i,E_e,E_field,E_total, vi_cm,ve_cm;

  printf("x0^3=%e \nn0i=%e \nlambda/hx=%e \nTemp = %e\n", x0*x0*x0, ni03D, lambda_D/hx,Te);
  //printf("dt=%e \nmax_SPe_dt=%d  \n",dt_emision/t_0,max_SPi_dt);
  //printf("Energia=%e \n",Et0);

                      
  //k_max_inj=t_0/dt;               
  int Kemision=10;
  double dt_emision=Kemision*dt;

  max_SPe_dt=NTSPe*dt_emision;       
  max_SPi_dt=max_SPe_dt;


  // Ciclo de tiempo

  k_max_inj=t_0/dt;
  K_total=Ntv*k_max_inj;
  int K_total_leapfrog=2*K_total;

  int kk=0;


  clock_t tiempo0 = clock();
  for (kt=0;kt<=K_total;kt++)
  {

    if (kt%10000==0)
    {
      printf("kt=%d\n",kt);
      printf("le=%d   li=%d \n",le, li );
    }

    if(kt<=k_max_inj && kt==kk)                   // Inyectar superpartículas (i-e)
    {        
      Cond_inic ();      // Definir posiciones y velocidades iniciales de los electrones e Iones durante el tiempo de inyección
      kk=kk+Kemision;  
    }

          //-----------------------------------------------
          // Calculo de "densidad de carga 2D del plasma"

    Concentration (pos_e,ne,le);           // Calcular concentración de superpartículas electrónicas
    Concentration (pos_i,ni,li);           // Calcular concentración de superpartículas Iónicas
          

    for (int j = 0; j < J_X; j++) 
    {
        for (int k = 0; k < J_Y; k++) 
        {
          rho[j][k]= cte_rho* Factor_carga_e*(ni[j][k]/n_0- ne[j][k]/n_0);
        }
    }

    // Calcular potencial eléctrico en puntos de malla
    Poisson2D_DirichletX_PeriodicY(phi,rho);

    // Calcular campo eléctrico en puntos de malla
    Electric_Field(phi,E_X, E_Y);

    // Avanzar posiciones de superpartículas electrónicas e Iónicas 
    Motion(pos_e,vel_e,le,electrones);           // Calcular nuevas posiciones de SP electrónicas
    Motion(pos_i,vel_i,li,Iones);                // Calcular nuevas posiciones de SP Iónicas
   
    //Cálculo de energías.

    if(kt%2000==0&&kt>0)// && kt > 0*k_max_inj)
      { 
          E_i=0; //Se inicializan las variables de la energía
          E_e=0;    
          E_field=0;
          E_total=0;
          E_perdida = 0;

          for (int y=0;y<li;y++)
            { 
              E_i = E_i + pow(sqrt(vel_i[y][0]*vel_i[y][0]+vel_i[y][1]*vel_i[y][1]),2)/(M_PI);
            }

          for (int y=0;y<le;y++)
            { 
              E_e = E_e + pow(sqrt(vel_e[y][0]*vel_e[y][0]+vel_e[y][1]*vel_e[y][1]),2)/(M_PI*razon_masas);
            }

          //Energía del campo

          for (int r=0;r<J_X;r++)
          {
            for(int ry=0;ry<J_Y;ry++)
            {
              E_field = E_field +  (ni[r][ry] -ne[r][ry])*phi[r][ry]*hx*hx; 
            }
          }

        
          E_field = 2*E_field /(M_PI);  
          
          //energia total del sistema
          E_total = E_field + E_i + E_e;

          //evolución temporal del centro de masa electrónico.
          ve_cm = vi_cm = 0; 
          /*for(int s=0; s<le;s++)
          {  
            ve_cm += sqrt(vel_e[s][0]*vel_e[s][0]+vel_e[s][1]*vel_e[s][1]); 
            vi_cm += sqrt(vel_i[s][0]*vel_i[s][0]+vel_i[s][1]*vel_i[s][1]); 
          }*/
           
          ve_cm = ve_cm /le; 
          vi_cm = vi_cm/li;

          //Energia perdida por partícula pérdida
          for (int q =0; q<perd;q++)
            E_perdida =  E_perdida + pow(vxe_perd[q],2)/(M_PI*razon_masas);

            //E_perdida = E_perdida/perd; 
          
          //if(kt != 0)

          //clock_t tiempo1 = clock();
          fprintf(outEnergia,"%e %e %e %e %e  %e  %e %d  %e \n", kt*dt, E_total, E_i,E_e,E_field, ve_cm, vi_cm, total_e_perdidos, E_perdida );
          //printf("Out Energia\n");
          
          //cout << " CPU time Full = " << double(tiempo1 - tiempo0) / CLOCKS_PER_SEC<< " sec" << endl;          
      }//cierre de calculo de energia
      clock_t tiempo1 = clock();
      if(kt%1000==0)
      {
        cout << " CPU time 1000 = " << double(tiempo1 - tiempo0) / CLOCKS_PER_SEC<< " sec" << endl;
        tiempo0 = clock();
      }
    
    //Salida de función de distribución

    if(kt==50000)
    {
      Funcion_Distribucion(pos_e,vel_e,le, (char *)"fdist-ele05x",  (char *)"fdist-ele05y");
      Funcion_Distribucion(pos_i,vel_i,li, (char *)"fdist-ion05x",  (char *)"fdist-ion05y");
    }

    if(kt==100000)
    {
      Funcion_Distribucion(pos_e,vel_e,le, (char *)"fdist-ele10x",  (char *)"fdist-ele10y");
      Funcion_Distribucion(pos_i,vel_i,li, (char *)"fdist-ion10x",  (char *)"fdist-ion10y");
    } 

    if(kt==150000)
    {
      Funcion_Distribucion(pos_e,vel_e,le, (char *)"fdist-ele15x",  (char *)"fdist-ele15y");
      Funcion_Distribucion(pos_i,vel_i,li, (char *)"fdist-ion15x",  (char *)"fdist-ion15y");
    } 

  }

   
  fclose(outEnergia);
  fclose(outPot0_6);
  fclose(outPot0_9);
  fclose(outPot1_5);
  fclose(outPot3_5);
  fclose(outPot5_5); 
  fclose(outPot15);
  fclose(outPot19);
  for(int i=0;i<=60;i++)
  {
    fclose(outFase_ele[i]);
    fclose(outFase_ion[i]);
  }

  return (0);
}// FINAL MAIN

// Función de Inyección de partículas

void Cond_inic (void) 
{  
   for (int i=0;i<max_SPe_dt;i++)
   {
     pos_e[i+le][0]=0.0; 
     vel_e[i+le][0]= distrib_vel_X (fe_Maxwell[0],vphi_e[0]);
     pos_e[i+le][1]=L_max[1]/2.0;
     vel_e[i+le][1]=distrib_vel_Y(fe_Maxwell[1],vphi_e[1]);
   }
 
   le=le+max_SPe_dt;

   for (int i=0;i<max_SPi_dt;i++)
   {
     pos_i[i+li][0]=0.0; 
     vel_i[i+li][0]=distrib_vel_X (fi_Maxwell[0],vphi_i[0]);
     pos_i[i+li][1]=L_max[1]/2.0;
     vel_i[i+li][1]=distrib_vel_Y (fi_Maxwell[1],vphi_i[1]);
   }
 
   li=li+max_SPi_dt;
}

double distrib_vel_X(double fmax,double vphi) // función para generar distribución semi-maxwelliana de velocidades de las particulas 
                                             // (Ver pág. 291 Computational Physics Fitzpatrick: Distribution functions--> Rejection Method)
{ 
                                          
  double sigma=vphi;                           // sigma=vflujo=vth    ( "dispersión" de la distribución Maxweliana)
  double vmin= 0. ;                            // Rapidez mínima  
  double vmax= 4.*sigma;                       // Rapidez máxima
  double v,f,f_random;

  static int flag = 0;
  if (flag == 0)
  {
    int seed = time (NULL);
    srand (seed);
    flag = 1;
  }  
  v=vmin+(vmax-vmin)*double(rand())/double(RAND_MAX); // Calcular valor aleatorio de v uniformemente distribuido en el rango [vmin,vmax]
  f =fmax*exp(-(1.0/M_PI)*pow(v/vphi,2));     //                       

  if (flag == 0)
  {
    int seed = time (NULL);
    srand (seed);
    flag = 1;
  }  

  f_random = fmax*double(rand())/double(RAND_MAX);    // Calcular valor aleatorio de f uniformemente distribuido en el rango [0,fmax]

  if (f_random > f) return distrib_vel_X(fmax,vphi);
  else return  v;
}

double distrib_vel_Y(double fmax,double vphi) // función para generar distribución semi-maxwelliana de velocidades de las particulas 
                                             // (Ver pág. 291 Computational Physics Fitzpatrick: Distribution functions--> Rejection Method)
{                                            
  double sigma=vphi;                           // sigma=vflujo=vth    ( "dispersión" de la distribución Maxweliana)
  double vmin= -3.*sigma;                            // Rapidez mínima  
  double vmax= 3.*sigma;                       // Rapidez máxima
  double v,f,f_random;

  static int flag = 0;
  if (flag == 0)
  {
    int seed = time (NULL);
    srand (seed);
    flag = 1;
  }  

  v=vmin+(vmax-vmin)*double(rand())/double(RAND_MAX); // Calcular valor aleatorio de v uniformemente distribuido en el rango [vmin,vmax]
  f =fmax*exp(-(1.0/M_PI)*pow(v/vphi,2));     //                       

  if (flag == 0)
  {
    int seed = time (NULL);
    srand (seed);
    flag = 1;
  }  

  f_random = fmax*double(rand())/double(RAND_MAX);    // Calcular valor aleatorio de f uniformemente distribuido en el rango [0,fmax]

  if (f_random > f) return distrib_vel_Y(fmax,vphi);
  else return  v;
}

double distrib_vel_Y_temp(double fmax,double vphi, int distemp) // función para generar distribución semi-maxwelliana de velocidades de las particulas 
                                             // (Ver pág. 291 Computational Physics Fitzpatrick: Distribution functions--> Rejection Method)
{                                             
double v=2*vphi*pow(-1,distemp);
return v;
}

//**********************************************************************************************
void Concentration (double pos[max_SPe][2], double n[J_X][J_Y],int NSP)
{ 
  int j_x,j_y;
  double temp_x,temp_y;
  float jr_x,jr_y;
  for (j_x=0; j_x<J_X; j_x++)
    {
      for (j_y=0; j_y<J_Y; j_y++)
      {
        n[j_x][j_y] = 0.;
      }
    } // Inicializar densidad de carga

  for (int i=0;i<NSP;i++)
    {
       jr_x=pos[i][0]/hx;       // indice (real) de la posición de la superpartícula
       j_x =int(jr_x);       // indice  inferior (entero) de la celda que contiene a la superpartícula
       temp_x = jr_x-j_x;
       jr_y=pos[i][1]/hx;       // indice (real) de la posición de la superpartícula
       j_y =int(jr_y);       // indice  inferior (entero) de la celda que contiene a la superpartícula
       temp_y = jr_y-j_y;

       n[j_x][j_y] += (1.-temp_x)*(1.-temp_y)/(hx*hx*hx);
       n[j_x+1][j_y] += temp_x*(1.-temp_y)/(hx*hx*hx);
       n[j_x][j_y+1] += (1.-temp_x)*temp_y/(hx*hx*hx);
       n[j_x+1][j_y+1] += temp_x*temp_y/(hx*hx*hx);
    }
}

//***************************************************

void Poisson2D_DirichletX_PeriodicY(double phi[J_X][J_Y],complex <double> rho[J_X][J_Y])
{
    int M=J_X,N=J_Y;
    double h = hx;
    double hy = hx;
    double *f;
    fftw_complex *f2;
    fftw_plan p,p_y,p_i,p_yi;
    f= (double*) malloc(sizeof(double)* 2*(M+1));
    f2 = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) *N);

    p = fftw_plan_r2r_1d(M, f, f, FFTW_RODFT00, FFTW_ESTIMATE);
    p_y = fftw_plan_dft_1d(N, f2, f2, FFTW_FORWARD, FFTW_ESTIMATE);    
    p_i = fftw_plan_r2r_1d(M, f, f, FFTW_RODFT00, FFTW_ESTIMATE);
    p_yi = fftw_plan_dft_1d(N, f2, f2, FFTW_BACKWARD, FFTW_ESTIMATE);

    // Llenado de Filas FFT
    for (int j = 0; j < M; j++) {
        for (int k = 0; k < N; k++)
            memcpy( &f2[k], &rho[j][k], sizeof( fftw_complex ) ); 
        fftw_execute(p_y);
        for (int k = 0; k < N; k++)
            memcpy( &rho[j][k], &f2[k], sizeof( fftw_complex ) );
    }
    // Llenado de Columnas FFT
    for (int k = 0; k < N; k++) {
        for (int j = 0; j < M; j++)
            f[j]=rho[j][k].real();
        fftw_execute(p);
        for (int j = 0; j < M; j++)
            rho[j][k].real()=f[j];
    }

    complex<double> i(0.0, 1.0);
    double pi = M_PI;
    complex<double> Wy = exp(2.0 * pi * i / double(N));
    complex<double> Wn = 1.0;
    for (int m = 0; m < M; m++) 
    {
        for (int n = 0; n < N; n++)
         {
            complex<double> denom = h*h*2.0+hy*hy*2.0;;
            denom -= hy*hy*(2.*cos((m+1)*pi/(M+1))) + h*h*(Wn + 1.0 / Wn);
            if (denom != 0.0)
                    rho[m][n] *= h*h*hy*hy / denom;
            Wn *= Wy;
        }
    }

    // Inversa de las filas
    for (int j = 0; j < M; j++)
    {
        for (int k = 0; k < N; k++)
            memcpy( &f2[k], &rho[j][k], sizeof( fftw_complex ) );
        fftw_execute(p_yi);
        for (int k = 0; k < N; k++)
        {
            memcpy( &rho[j][k], &f2[k], sizeof( fftw_complex ) );
            rho[j][k] /= double(N); //Normalización por DFT inversa.
        }
    }
    // Inversa de las columnas
    for (int k = 0; k < N; k++) 
    {

        for (int j = 0; j < M; j++)
            memcpy( &f[j], &rho[j][k], sizeof( fftw_complex ) );
        fftw_execute(p_i);
        for (int j = 0; j < M; j++)
        {
            phi[j][k]=f[j]/double(2*(M+1)); //Salida de Potencial a la matriz Phi, Normalización por DST.
        }

    }

    for (int k = 0; k < N; k++) 
    {
      phi[0][k]=0;
      phi[M-1][k]=0;
    }


    if(kt%10000==0)
    {
  
      // Escribir a archivo
      cout << " Potential in file PoissonFFTW3.data" << endl;
      ofstream dataFile("PoissonFFTW3.data");
      for (int j = 0; j < M; j++) {
          double thisx = j * h;
          for (int k = 0; k < N; k++) {
              double thisy = k * hy;
              dataFile << thisx << '\t' << thisy << '\t' << phi[j][k] << '\n';
          }
          dataFile << '\n';
      }
      dataFile.close();
    }
    fftw_destroy_plan(p);
    fftw_destroy_plan(p_i);
    fftw_destroy_plan(p_y);
    fftw_destroy_plan(p_yi);
    free(f); fftw_free(f2);
}

void Poisson2D_DirichletX_DirichletY(double phi[J_X][J_Y],complex <double> rho[J_X][J_Y])
{
    int M=J_X,N=J_Y;
    double h = hx;
    double hy = hx;
    double *f,*f2;
    fftw_plan p,p_y,p_i,p_yi;
    f= (double*) malloc(sizeof(double)* 2*(M+1));
    f2= (double*) malloc(sizeof(double)* 2*(N+1));

    p = fftw_plan_r2r_1d(M, f, f, FFTW_RODFT00, FFTW_ESTIMATE); 
    p_y = fftw_plan_r2r_1d(N, f2, f2, FFTW_RODFT00, FFTW_ESTIMATE);     
    p_i = fftw_plan_r2r_1d(M, f, f, FFTW_RODFT00, FFTW_ESTIMATE);
    p_yi = fftw_plan_r2r_1d(N, f2, f2, FFTW_RODFT00, FFTW_ESTIMATE);

    // Llenado de Filas FFT
    for (int j = 0; j < M; j++) {
        for (int k = 0; k < N; k++)
            memcpy( &f2[k], &rho[j][k], sizeof( fftw_complex ) ); 
        fftw_execute(p_y);
        for (int k = 0; k < N; k++)
            memcpy( &rho[j][k], &f2[k], sizeof( fftw_complex ) );
    }
    // Llenado de Columnas FFT
    for (int k = 0; k < N; k++) {
        for (int j = 0; j < M; j++)
            f[j]=rho[j][k].real();
        fftw_execute(p);
        for (int j = 0; j < M; j++)
            rho[j][k].real()=f[j];
    }

    complex<double> i(0.0, 1.0);
    double pi = M_PI;
    for (int m = 0; m < M; m++) 
    {
        for (int n = 0; n < N; n++)
         {
            complex<double> denom = h*h*2.0+hy*hy*2.0;;
            denom-=   hy*hy*(2*cos((m+1)*pi/(M+1))) + h*h*(2*cos((n+1)*pi/(N+1)));
            if (denom != 0.0)
                    rho[m][n] *= h*h*hy*hy / denom;
        }
    }


    // Inversa de las filas
    for (int j = 0; j < M; j++)
    {
        for (int k = 0; k < N; k++)
            memcpy( &f2[k], &rho[j][k], sizeof( fftw_complex ) );
        fftw_execute(p_yi);
        for (int k = 0; k < N; k++)
        {
            memcpy( &rho[j][k], &f2[k], sizeof( fftw_complex ) );
            rho[j][k] /= double(2*(N+1)); //Normalización por DST.
        }
    }
    // Inversa de las columnas
    for (int k = 0; k < N; k++) 
    {

        for (int j = 0; j < M; j++)
            memcpy( &f[j], &rho[j][k], sizeof( fftw_complex ) );
        fftw_execute(p_i);
        for (int j = 0; j < M; j++)
        {
            phi[j][k]=f[j]/double(2*(M+1)); //Normalización por DST.
        }

    }

    for (int k = 0; k < N; k++) 
    {
      phi[0][k]=0;
      phi[M-1][k]=0;
    }


    if(kt%10000==0)
    {
  
      // Escribir a archivo
      cout << " Potential in file PoissonFFTW3.data" << endl;
      ofstream dataFile("PoissonFFTW3.data");
      for (int j = 0; j < M; j++) {
          double thisx = j * h;
          for (int k = 0; k < N; k++) {
              double thisy = k * hy;
              dataFile << thisx << '\t' << thisy << '\t' << phi[j][k] << '\n';
          }
          dataFile << '\n';
      }
      dataFile.close();
    }
    fftw_destroy_plan(p);
    fftw_destroy_plan(p_i);
    fftw_destroy_plan(p_y);
    fftw_destroy_plan(p_yi);
    free(f); fftw_free(f2);
}

//*****************************

void Electric_Field (double phi[J_X][J_Y], double E_X[J_X][J_Y], double E_Y[J_X][J_Y])                 // Función para calcular el campo eléctrico en los puntos de malla
{                                                                             // a partir del potencial. 
  for (int j=1;j<J_X-2;j++)
  {
      for (int k=1;k<J_Y-2;k++)
      {
        E_X[j][k]=(phi[j-1][k]-phi[j+1][k])/(2.*hx);
        E_Y[j][k]=(phi[j][k-1]-phi[j][k+1])/(2.*hx);

        E_X[0][k]=0;  //Cero en las fronteras X
        E_Y[0][k]=0;
        E_X[J_X-1][k]=0; 
        E_Y[J_X-1][k]=0;
      }          

      E_X[j][0]=(phi[j-1][0]-phi[j+1][0])/(2.*hx); 
      E_Y[j][0]=(phi[j][J_Y-1]-phi[j][1])/(2.*hx);
        
      E_X[j][J_Y-1]=(phi[j-1][J_Y-1]-phi[j+1][J_Y-1])/(2.*hx); 
      E_Y[j][J_Y-1]=(phi[j][J_Y-2]-phi[j][0])/(2.*hx);
  }
}


void  Motion(double pos[max_SPe][2],  double vel[max_SPe][2],  int NSP, int especie)
{
   int j_x,j_y;
   double temp_x,temp_y,Ep_X, Ep_Y,fact;
   float jr_x,jr_y;
   int kk1=0;
   int ind;
   int conteo_perdidas=0;
   
   if(especie==electrones)
    fact=fact_el;                    
    
   if(especie==Iones)
    fact=fact_i;
    
   for (int i=0;i<NSP;i++)
    {
       jr_x=pos[i][X]/hx;     // Índice (real) de la posición de la superpartícula (X)
       j_x =int(jr_x);        // Índice  inferior (entero) de la celda que contiene a la superpartícula (X)
       temp_x = jr_x-j_x;
       jr_y=pos[i][Y]/hx;     // Índice (real) de la posición de la superpartícula (Y)
       j_y =int(jr_y);        // Índice  inferior (entero) de la celda que contiene a la superpartícula (Y)
       temp_y = jr_y-j_y;


       Ep_X=(1-temp_x)*(1-temp_y)*E_X[j_x][j_y]+temp_x*(1-temp_y)*E_X[j_x+1][j_y]+(1-temp_x)*temp_y*E_X[j_x][j_y+1]+temp_x*temp_y*E_X[j_x+1][j_y+1];
       Ep_Y=(1-temp_x)*(1-temp_y)*E_Y[j_x][j_y]+temp_x*(1-temp_y)*E_Y[j_x+1][j_y]+(1-temp_x)*temp_y*E_Y[j_x][j_y+1]+temp_x*temp_y*E_Y[j_x+1][j_y+1];
       pos[i][X]=pos[i][X]+vel[i][X]*dt;
       pos[i][Y]=pos[i][Y]+vel[i][Y]*dt;

       vel[i][X]=vel[i][X]+cte_E*Factor_carga_e*fact*Ep_X*dt;
       vel[i][Y]=vel[i][Y]+cte_E*Factor_carga_e*fact*Ep_Y*dt;


       if(pos[i][X]<0) //Rebote en la pared del material.
       {
          pos[i][X]=-pos[i][X];
          vel[i][X]=-vel[i][X];
       }

       if (pos[i][X]>L_max[X]) //Partícula fuera del espacio de Simulación
       {    
            conteo_perdidas++;
            if(especie==electrones)
            {
              total_e_perdidos++;
              printf("Electron perdido No. %d,  i=%d, kt=%d \n",total_e_perdidos, i ,kt);
              mv2perdidas+=pow( sqrt(vel[i][X]*vel[i][X]+vel[i][Y]*vel[i][Y]) , 2);
            }
            else
            {
              total_i_perdidos++;
              printf("Ion perdido No. %d,  i=%d, kt=%d \n",total_i_perdidos, i ,kt);
              mv2perdidas+=pow( sqrt(vel[i][X]*vel[i][X]+vel[i][Y]*vel[i][Y]) , 2);
            }
            /*
            ind = 0;
            cont_per = 0;
            if(perd==0)
            {
                perdidas[perd]=i;
                vxe_perd[perd] = sqrt(vel[i][0]*vel[i][0]+vel[i][1]*vel[i][1]);
                perd++;
            }
            for(int k=0; k<perd; k++)
            {
                if(perdidas[k] != i ) 
                cont_per ++; 
            }
                 
            if(cont_per==perd) 
            { 
                perdidas[perd]=i; 
                vxe_perd[perd] = sqrt(vel[i][0]*vel[i][0]+vel[i][1]*vel[i][1]);
                perd ++;
                printf("Particula perdida= %d, especie=%d,  i=%d, kt=%d \n", perd, especie, i ,kt);
          } */
         
       }


       while(pos[i][Y]>=L_max[Y]) //Ciclo en el eje Y.
       {
          pos[i][Y]=pos[i][Y]-L_max[Y];
       }

       while(pos[i][Y]<0.0) //Ciclo en el eje Y.
       {

          pos[i][Y]=L_max[Y]+pos[i][Y];
       }



       if(pos[i][X]>=0 && pos[i][X]<=L_max[X])
        {
            kk1=kk1+1;
            pos[kk1-1][X]=pos[i][X]; 
            pos[kk1-1][Y]=pos[i][Y];
            vel[kk1-1][X]=vel[i][X]; 
            vel[kk1-1][Y]=vel[i][Y];
        }

       //Salida espacio de Fase

       if(kt%10000==0&&especie==electrones)
          {
              fprintf(outFase_ele[kt/10000]," %e   %e  %e  %e  %e \n",kt*dt,pos[i][X],vel[i][X],pos[i][Y],vel[i][Y]);
              //printf("Fase out\n");
          }

       if(kt%10000==0&&especie==Iones)
          {
              fprintf(outFase_ion[kt/10000]," %e   %e  %e  %e  %e \n",kt*dt,pos[i][X],vel[i][X],pos[i][Y],vel[i][Y]);
          }

    }

    if(especie==electrones)
    {
      le=le-conteo_perdidas;
    }
    else
    {
      li=li-conteo_perdidas;
    }
}

void Funcion_Distribucion(double pos[max_SPe][2], double vel[max_SPe][2] , int NSP, char *archivo_X, char *archivo_Y)
{   
    double Nc=30;
    FILE *pFile[2];
    pFile[0] = fopen(archivo_X,"w"); pFile[1] = fopen(archivo_Y,"w");
    int suma=0;
    int ind=0;
    double a;

    // Calcular el valor minimo y la maximo de la velocidad
    for(int i=0;i<2;i++)
    {
        double max=0;
        double min=0; 
        double dv;
        for (int h=0;h<NSP;h++)
        {
          if(vel[h][i]<min)
          {
            min=vel[h][i]; //valor minimo en velocidades
          }

          if(vel[h][i]>max)
          {
            max=vel[h][i];//valor maximo en velocidades
          }   
        }
    
        dv = (max-min)/Nc;
        a=min;//inicializar el contador en velocidades
      

        printf("min=%e max=%e dv= %e kt=%d #Particulas = %d ", min,max, dv,kt, NSP);
        for(ind=0; ind < Nc;ind++) 
        {
          suma =0;
          for (int j=0;j<NSP;j++)
            {
                if(a <= vel[j][i] && vel[j][i] < a + dv)
                  suma++;
            }
          fprintf(pFile[i]," %e  %d  \n", a, suma);
          a = a + dv;
        }        
      fclose(pFile[i]); 
    }
}

/*
void  update_Positions(double pos[max_SPe][2],  double vel[max_SPe][2],  int NSP, int especie)
{
  for(int i=0;i<NSP;i++)
  {
    pos[i][X]=pos[i][X]+vel[i][X]*dt;
    pos[i][Y]=pos[i][Y]+vel[i][Y]*dt;


    if(pos[i][X]<0) //Rebote en la pared del material.
       {
          pos[i][X]=-pos[i][X];
          vel[i][X]=-vel[i][X];
       }

    if (pos[i][X]>L_max[X]) //Partícula fuera del espacio de Simulación
       {  
            if(especie==electrones)
              le=le-1;
            else
              li=li-1;

            ind = 0;
            cont_per = 0;
            if(perd==0)
            {
                perdidas[perd]=i;
                vxe_perd[perd] = sqrt(vel[i][0]*vel[i][0]+vel[i][1]*vel[i][1]);
                perd++;
            }
            for(int k=0; k<perd; k++)
            {
                if(perdidas[k] != i ) 
                cont_per ++; 
            }
                 
            if(cont_per==perd) 
            { 
                perdidas[perd]=i; 
                vxe_perd[perd] = sqrt(vel[i][0]*vel[i][0]+vel[i][1]*vel[i][1]);
                perd ++;
                printf("Particula perdida= %d, especie=%d,  i=%d, kt=%d \n", perd, especie, i ,kt);
          } 
         
       }


    while(pos[i][1]>L_max[1]) //Ciclo en el eje Y.
       {
          pos[i][1]=pos[i][1]-L_max[1];
       }

    while(pos[i][1]<0) //Ciclo en el eje Y.
       {

          pos[i][1]=L_max[1]+pos[i][1];
       }



    if(pos[i][X]>=0 && pos[i][X]<=L_max[0])
        {
            kk1=kk1+1;
            pos[kk1-1][X]=pos[i][X]; 
            pos[kk1-1][Y]=pos[i][Y];
            vel[kk1-1][X]=vel[i][X]; 
            vel[kk1-1][Y]=vel[i][Y];
        }

  }


}

void  update_Velocities(double vel[max_SPe][2],  E[J_X][J_Y], int especie)
{
  if(especie==electrones)
    fact=fact_el;                    
    
  if(especie==Iones)
    fact=fact_i;

  Ep_X=(1-temp_x)*(1-temp_y)*E_X[j_x][j_y]+temp_x*(1-temp_y)*E_X[j_x+1][j_y]+(1-temp_x)*temp_y*E_X[j_x][j_y+1]+temp_x*temp_y*E_X[j_x+1][j_y+1];
  Ep_Y=(1-temp_x)*(1-temp_y)*E_Y[j_x][j_y]+temp_x*(1-temp_y)*E_Y[j_x+1][j_y]+(1-temp_x)*temp_y*E_Y[j_x][j_y+1]+temp_x*temp_y*E_Y[j_x+1][j_y+1];

  vel[i][X]=vel[i][X]+cte_E*Factor_carga_e*fact*Ep_X*dt;
  vel[i][Y]=vel[i][Y]+cte_E*Factor_carga_e*fact*Ep_Y*dt;


}
*/
