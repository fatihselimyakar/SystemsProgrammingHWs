#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <sys/wait.h>

#define BUFFER_SIZE 999999
#define NR_END 1
#define FREE_ARG char*
#define SIGN(a,b) ((b) >= 0.0 ? fabs(a) : -fabs(a))
static double dmaxarg1,dmaxarg2;
#define DMAX(a,b) (dmaxarg1=(a),dmaxarg2=(b),(dmaxarg1) > (dmaxarg2) ?\
(dmaxarg1) : (dmaxarg2))
static int iminarg1,iminarg2;
#define IMIN(a,b) (iminarg1=(a),iminarg2=(b),(iminarg1) < (iminarg2) ?\
(iminarg1) : (iminarg2))

double **dmatrix(int nrl, int nrh, int ncl, int nch)
/* allocate a double matrix with subscript range m[nrl..nrh][ncl..nch] */
{
	int i,nrow=nrh-nrl+1,ncol=nch-ncl+1;
	double **m;
	/* allocate pointers to rows */
	m=(double **) malloc((size_t)((nrow+NR_END)*sizeof(double*)));
	m += NR_END;
	m -= nrl;
	/* allocate rows and set pointers to them */
	m[nrl]=(double *) malloc((size_t)((nrow*ncol+NR_END)*sizeof(double)));
	m[nrl] += NR_END;
	m[nrl] -= ncl;
	for(i=nrl+1;i<=nrh;i++) m[i]=m[i-1]+ncol;
	/* return pointer to array of pointers to rows */
	return m;
}

double *dvector(int nl, int nh)
/* allocate a double vector with subscript range v[nl..nh] */
{
	double *v;
	v=(double *)malloc((size_t) ((nh-nl+1+NR_END)*sizeof(double)));
	return v-nl+NR_END;
}

void free_dvector(double *v, int nl, int nh)
/* free a double vector allocated with dvector() */
{
	free((FREE_ARG) (v+nl-NR_END));
}

double pythag(double a, double b)
/* compute (a2 + b2)^1/2 without destructive underflow or overflow */
{
	double absa,absb;
	absa=fabs(a);
	absb=fabs(b);
	if (absa > absb) return absa*sqrt(1.0+(absb/absa)*(absb/absa));
	else return (absb == 0.0 ? 0.0 : absb*sqrt(1.0+(absa/absb)*(absa/absb)));
}

/******************************************************************************/
void svdcmp(double **a, int m, int n, double w[], double **v)
/*******************************************************************************
Given a matrix a[1..m][1..n], this routine computes its singular value
decomposition, A = U.W.VT.  The matrix U replaces a on output.  The diagonal
matrix of singular values W is output as a vector w[1..n].  The matrix V (not
the transpose VT) is output as v[1..n][1..n].
*******************************************************************************/
{
	int flag,i,its,j,jj,k,l,nm;
	double anorm,c,f,g,h,s,scale,x,y,z,*rv1;

	rv1=dvector(1,n);
	g=scale=anorm=0.0; /* Householder reduction to bidiagonal form */
	for (i=1;i<=n;i++) {
		l=i+1;
		rv1[i]=scale*g;
		g=s=scale=0.0;
		if (i <= m) {
			for (k=i;k<=m;k++) scale += fabs(a[k][i]);
			if (scale) {
				for (k=i;k<=m;k++) {
					a[k][i] /= scale;
					s += a[k][i]*a[k][i];
				}
				f=a[i][i];
				g = -SIGN(sqrt(s),f);
				h=f*g-s;
				a[i][i]=f-g;
				for (j=l;j<=n;j++) {
					for (s=0.0,k=i;k<=m;k++) s += a[k][i]*a[k][j];
					f=s/h;
					for (k=i;k<=m;k++) a[k][j] += f*a[k][i];
				}
				for (k=i;k<=m;k++) a[k][i] *= scale;
			}
		}
		w[i]=scale *g;
		g=s=scale=0.0;
		if (i <= m && i != n) {
			for (k=l;k<=n;k++) scale += fabs(a[i][k]);
			if (scale) {
				for (k=l;k<=n;k++) {
					a[i][k] /= scale;
					s += a[i][k]*a[i][k];
				}
				f=a[i][l];
				g = -SIGN(sqrt(s),f);
				h=f*g-s;
				a[i][l]=f-g;
				for (k=l;k<=n;k++) rv1[k]=a[i][k]/h;
				for (j=l;j<=m;j++) {
					for (s=0.0,k=l;k<=n;k++) s += a[j][k]*a[i][k];
					for (k=l;k<=n;k++) a[j][k] += s*rv1[k];
				}
				for (k=l;k<=n;k++) a[i][k] *= scale;
			}
		}
		anorm = DMAX(anorm,(fabs(w[i])+fabs(rv1[i])));
	}
	for (i=n;i>=1;i--) { /* Accumulation of right-hand transformations. */
		if (i < n) {
			if (g) {
				for (j=l;j<=n;j++) /* Double division to avoid possible underflow. */
					v[j][i]=(a[i][j]/a[i][l])/g;
				for (j=l;j<=n;j++) {
					for (s=0.0,k=l;k<=n;k++) s += a[i][k]*v[k][j];
					for (k=l;k<=n;k++) v[k][j] += s*v[k][i];
				}
			}
			for (j=l;j<=n;j++) v[i][j]=v[j][i]=0.0;
		}
		v[i][i]=1.0;
		g=rv1[i];
		l=i;
	}
	for (i=IMIN(m,n);i>=1;i--) { /* Accumulation of left-hand transformations. */
		l=i+1;
		g=w[i];
		for (j=l;j<=n;j++) a[i][j]=0.0;
		if (g) {
			g=1.0/g;
			for (j=l;j<=n;j++) {
				for (s=0.0,k=l;k<=m;k++) s += a[k][i]*a[k][j];
				f=(s/a[i][i])*g;
				for (k=i;k<=m;k++) a[k][j] += f*a[k][i];
			}
			for (j=i;j<=m;j++) a[j][i] *= g;
		} else for (j=i;j<=m;j++) a[j][i]=0.0;
		++a[i][i];
	}
	for (k=n;k>=1;k--) { /* Diagonalization of the bidiagonal form. */
		for (its=1;its<=30;its++) {
			flag=1;
			for (l=k;l>=1;l--) { /* Test for splitting. */
				nm=l-1; /* Note that rv1[1] is always zero. */
				if ((double)(fabs(rv1[l])+anorm) == anorm) {
					flag=0;
					break;
				}
				if ((double)(fabs(w[nm])+anorm) == anorm) break;
			}
			if (flag) {
				c=0.0; /* Cancellation of rv1[l], if l > 1. */
				s=1.0;
				for (i=l;i<=k;i++) {
					f=s*rv1[i];
					rv1[i]=c*rv1[i];
					if ((double)(fabs(f)+anorm) == anorm) break;
					g=w[i];
					h=pythag(f,g);
					w[i]=h;
					h=1.0/h;
					c=g*h;
					s = -f*h;
					for (j=1;j<=m;j++) {
						y=a[j][nm];
						z=a[j][i];
						a[j][nm]=y*c+z*s;
						a[j][i]=z*c-y*s;
					}
				}
			}
			z=w[k];
			if (l == k) { /* Convergence. */
				if (z < 0.0) { /* Singular value is made nonnegative. */
					w[k] = -z;
					for (j=1;j<=n;j++) v[j][k] = -v[j][k];
				}
				break;
			}
			if (its == 30) printf("no convergence in 30 svdcmp iterations");
			x=w[l]; /* Shift from bottom 2-by-2 minor. */
			nm=k-1;
			y=w[nm];
			g=rv1[nm];
			h=rv1[k];
			f=((y-z)*(y+z)+(g-h)*(g+h))/(2.0*h*y);
			g=pythag(f,1.0);
			f=((x-z)*(x+z)+h*((y/(f+SIGN(g,f)))-h))/x;
			c=s=1.0; /* Next QR transformation: */
			for (j=l;j<=nm;j++) {
				i=j+1;
				g=rv1[i];
				y=w[i];
				h=s*g;
				g=c*g;
				z=pythag(f,h);
				rv1[j]=z;
				c=f/z;
				s=h/z;
				f=x*c+g*s;
				g = g*c-x*s;
				h=y*s;
				y *= c;
				for (jj=1;jj<=n;jj++) {
					x=v[jj][j];
					z=v[jj][i];
					v[jj][j]=x*c+z*s;
					v[jj][i]=z*c-x*s;
				}
				z=pythag(f,h);
				w[j]=z; /* Rotation can be arbitrary if z = 0. */
				if (z) {
					z=1.0/z;
					c=f*z;
					s=h*z;
				}
				f=c*g+s*y;
				x=c*y-s*g;
				for (jj=1;jj<=m;jj++) {
					y=a[jj][j];
					z=a[jj][i];
					a[jj][j]=y*c+z*s;
					a[jj][i]=z*c-y*s;
				}
			}
			rv1[l]=0.0;
			rv1[k]=f;
			w[k]=x;
		}
	}
	free_dvector(rv1,1,n);
}

/* Calculates the singular values by using singular value decomposition and prints the matrix C and singular values */
void calculate_singular_values(int** arr,int number){
    int n=pow(2,number)+1,j,i,k;
	double **a = (double **)malloc(n * sizeof(double *)); 
    for (i=0; i<n; i++) 
         a[i] = (double *)malloc(n* sizeof(double));
	
	double w[n];
	double **v= (double **)malloc(n * sizeof(double *)); 
    for (i=0; i<n; i++) 
         v[i] = (double *)malloc(n* sizeof(double));
	
    /* prints matrix C */
	printf("\n**Matrix C**\n");
	for(i=1;i<n;++i){
		for(j=1;j<n;++j){
			a[i][j]=arr[i-1][j-1];
            printf("%d ",(int)a[i][j]);
		}
        printf("\n");
	}
	
    /* Calculates the singular values */
	svdcmp(a,n-1,n-1,w,v);

	/* prints singular values */
    printf("\n**Singular Values**\n");
	for(i=1;i<n;++i){
        //if(w[i]!=0.0){
            printf("%.3f ",w[i]);
        //}
		    
	}
    printf("\n");

}

/* Reads with locking */
ssize_t read_lock(int fd, void *buf, size_t count){
    struct flock lock;
    memset(&lock,0,sizeof(lock));
    ssize_t ret_val;

    lock.l_type=F_RDLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("program | read_lock | fcntl error");
        close(fd); 
        exit(EXIT_FAILURE);
    }
    
    ret_val=read(fd,buf,count);
    

    lock.l_type=F_UNLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("program | read_lock | fcntl error");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    return ret_val;

}

/* Multiplicates the [row1]x[col1] matrix1 and [row2]x[col2] matrix2 then writes the given pipe */
void matrix_multiplication(int fd,int row1, int col1, int **matrix1,int row2, int col2, int **matrix2) 
{ 
    int x, i, j; 
    int **res = (int **)malloc(row1 * sizeof(int *)); 
    for (i=0; i<row1; i++) 
         res[i] = (int *)malloc(col2* sizeof(int));

    /* Matrix multiplication */
    for (i=0;i<row1;i++) { 
        for (j=0;j<col2;j++){ 
            res[i][j] = 0; 
            for (x=0;x<col1;x++){ 
                res[i][j]+=matrix1[i][x]*matrix2[x][j]; 
            } 
        } 
    } 

    /* Converts the 2d matrix to string */
    char* string=malloc(BUFFER_SIZE);
    char* character=malloc(1000);
    int size=0;
    for (i = 0; i < row1; i++) 
    { 
        for (j = 0; j < col2; j++) 
        { 
            sprintf(character,"%d ",res[i][j]);
            strcat(string,character);
        } 
        strcat(string,"\n");
    } 

    /* writes the given pipe */
    if(write(fd,string,strlen(string))<0){
        perror("write error in matrix multiplication");
        exit(EXIT_FAILURE);
    }
    free(character);
    free(string);
    free(res);
} 

/* Converts the string to 2d int array by seperating tokens using strtok */
int** char_to_int_array(char* quarter,int n){
    int r_and_c=pow(2,n)/2;
    int i,j;

    int **arr = (int **)malloc(r_and_c * sizeof(int *)); 
    for (i=0; i<r_and_c; i++) 
         arr[i] = (int *)malloc(r_and_c * sizeof(int));

    i=0,j=0;
    char* str = strtok(quarter," \n\0");
    for(i=0;i<r_and_c && str != NULL;++i){
        for(j=0;j<r_and_c && str != NULL;++j){
            arr[i][j]=atoi(str);
            str=strtok(NULL," \n\0");
        }
    }
    return arr;
}

/* Merges 4 quarter matrix into one big matrix */
int** merge_quarters(char* first_quarter,char* second_quarter,char* third_quarter,char* fourth_quarter,int n){
    int r_and_c=pow(2,n);
    int** q1=char_to_int_array(first_quarter,n);
    int** q2=char_to_int_array(second_quarter,n);
    int** q3=char_to_int_array(third_quarter,n);
    int** q4=char_to_int_array(fourth_quarter,n);
    int i,j;
    int **arr = (int **)malloc(r_and_c * sizeof(int *)); 
    for (i=0; i<r_and_c; ++i) 
         arr[i] = (int *)malloc(r_and_c * sizeof(int));

    /*first quarter localization*/
    for(i=0;i<r_and_c/2;++i){
        for(j=0;j<r_and_c/2;++j){
            arr[i][j]=q1[i][j];
        }
    }
    /*second quarter localization*/
    for(i=0;i<r_and_c/2;++i){
        for(j=0;j<r_and_c/2;++j){
            arr[i][j+r_and_c/2]=q2[i][j];
        }
    }
    /*third quarter localization*/
    for(i=0;i<r_and_c/2;++i){
        for(j=0;j<r_and_c/2;++j){
            arr[i+r_and_c/2][j]=q3[i][j];
        }
    }
    /*fourth quarter localization*/
    for(i=0;i<r_and_c/2;++i){
        for(j=0;j<r_and_c/2;++j){
            arr[i+r_and_c/2][j+r_and_c/2]=q4[i][j];
        }
    }

    free(q1);
    free(q2);
    free(q3);
    free(q4);

    return arr;
}

/* read [2^n]*[2^n] input in fd and returns 2d [2^n]X[2^n] integer array */
int** read_input(int n,int fd){
    int r_and_c=pow(2,n);
    int i,j; 
    char temp_char;
    int **arr = (int **)malloc(r_and_c * sizeof(int *)); 
    for (i=0; i<r_and_c; i++) 
         arr[i] = (int *)malloc(r_and_c * sizeof(int));

    for (i = 0; i < r_and_c; i++){
        for (j = 0; j < r_and_c; j++){
            if(read(fd,&temp_char,1)<=0){
                errno=EINVAL;
                perror("program | parent_read_input | read error");
                close(fd); 
                exit(EXIT_FAILURE);
            } 
            arr[i][j] = temp_char;
        }
    }
      
    return arr;
}

/* Finds and returns the desired quarter in the given matrix. */
int** find_quarter(int n,int** arr,int quarter){
    int r_and_c=pow(2,n);
    int quarter_r_and_c=r_and_c/2;
    int i,j;

    /*left,up quarter matrix */
    if(quarter==1){
        int **res_arr = (int **)malloc(r_and_c * sizeof(int *)); 
        for (i=0; i<r_and_c; i++) 
            res_arr[i] = (int *)malloc(quarter_r_and_c * sizeof(int));

        for(i=0;i<r_and_c;++i){
            for(j=0;j<quarter_r_and_c;++j){
                res_arr[i][j]=arr[i][j];
            }
        }

        return res_arr;
    }
    /* right,up quarter matrix */
    else if(quarter==2){
        int **res_arr = (int **)malloc(r_and_c * sizeof(int *)); 
        for (i=0; i<r_and_c; i++) 
            res_arr[i] = (int *)malloc(quarter_r_and_c * sizeof(int));

        for(i=0;i<r_and_c;++i){
            for(j=quarter_r_and_c;j<r_and_c;++j){
                res_arr[i][j-quarter_r_and_c]=arr[i][j];
            }
        }
        return res_arr;
    }
    /* left,down quarter matrix */
    else if(quarter==3){
        int **res_arr = (int **)malloc(quarter_r_and_c * sizeof(int *)); 
        for (i=0; i<quarter_r_and_c; i++) 
            res_arr[i] = (int *)malloc(r_and_c * sizeof(int));

        for(i=0;i<quarter_r_and_c;++i){
            for(j=0;j<r_and_c;++j){
                res_arr[i][j]=arr[i][j];
            }
        }
        return res_arr;
    }
    /* right,down quarter matrix */
    else{
        int **res_arr = (int **)malloc(quarter_r_and_c * sizeof(int *)); 
        for (i=0; i<quarter_r_and_c; i++) 
            res_arr[i] = (int *)malloc(r_and_c * sizeof(int));

        for(i=quarter_r_and_c;i<r_and_c;++i){
            for(j=0;j<r_and_c;++j){
                res_arr[i-quarter_r_and_c][j]=arr[i][j];
            }
        }
        return res_arr;
    }
}

/* SIGINT handler function */
void sigint_handler(int signum){
    printf("Signal SIGINT caught, exiting..\n");
    exit(EXIT_FAILURE);
}

/* SIGCHLD handler function */
void sigchld_handler(int signum){
    printf("SIGCHLD caught\n");
    
}

int main(int argc,char *argv[]){
    int opt;
    char ivalue[20];
    char jvalue[20];
    int n;
    struct sigaction sact,sa;
    sigemptyset(&sact.sa_mask); 
    sact.sa_flags = 0;
    sact.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask); 
    sa.sa_flags = SA_RESTART; //for avoiding interrupted system call error
    sa.sa_handler = sigchld_handler;

    /* sigaction for SIGINT */
    if (sigaction(SIGINT, &sact, NULL) != 0){
        perror("SIGINT sigaction() error");
        exit(EXIT_SUCCESS);
    }

    /* sigaction for SIGCHLD */
    if (sigaction(SIGCHLD, &sa, NULL) != 0){
        perror("SIGCHLD sigaction() error");
        exit(EXIT_SUCCESS);
    }

    while((opt = getopt(argc, argv, ":i:j:n:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'i':
                strcpy(ivalue,optarg);
                break;
            case 'j':
                strcpy(jvalue,optarg);
                break;
            case 'n':
                n=atoi(optarg);
                if(n<=0){
                    errno=EINVAL;
                    perror("program | main | n should be positive.");
                    exit(EXIT_FAILURE);
                }
                break;
            case ':':  
                errno=EINVAL;
                perror("program | main | You must write like this: \"./program -i inputPath -j inputPath -n positiveNumber\"");
                exit(EXIT_FAILURE);
                break;  
            case '?':  
                errno=EINVAL;
                perror("program | main | You must write like this: \"./program -i inputPath -j inputPath -n positiveNumber\"");
                exit(EXIT_FAILURE);
                break; 
            default:
                abort (); 
        }
    }

    if(optind!=7){
        errno=EINVAL;
        perror("program | main | You must write like this: \"./program -i inputPath -j inputPath -n positiveNumber\"");
        exit(EXIT_FAILURE);
    }

    printf("%s %s %d\n",ivalue,jvalue,n);

    /* opens the input files in read only mode */
    int fd_i,fd_j;
    if((fd_i=open(ivalue,O_RDONLY))<0 || (fd_j=open(jvalue,O_RDONLY))<0){
        perror("program | main | open error");
        exit(EXIT_FAILURE);
    }

    /* Parent process is starting */
    int r_and_c=pow(2,n);
    int pfd[8][2]; //8 times pfd[2] for piping
    int i,dummy,j,k;

    setbuf(stdout,NULL);
    printf("Parent/Process 1 (PID=%d) started\n",getpid());

    /* 8 different pipes open */
    for(i=0;i<8;++i){
        if(pipe(pfd[i]) == -1){
            perror("program | main | parent | pipe error");
            exit(EXIT_FAILURE);
        }
    }
    
    /* Child process creating and their calculation process'es */
    for(i=1,j=0;i<5|j<8;i++,j+=2){
        char matrix1[r_and_c*r_and_c];
        char matrix2[r_and_c*r_and_c];
        switch(fork()){
            /* if fork does not run then exits */
            case -1:
                perror("program | main | child | fork error");
                exit(EXIT_FAILURE);
            case 0:
                /* Closes the unused pipe ends */
                if(close(pfd[j][0])==-1 || close(pfd[j+1][1])==-1){
                    perror("program | main | child | close error");
                    exit(EXIT_FAILURE);
                } 

                int** int_matrix1=read_input(n,pfd[j+1][0]);
                int** int_matrix2=read_input(n,pfd[j+1][0]);
                /* process P2 */
                if(i==1){
                    int** first_quarter=find_quarter(n,int_matrix1,3);
                    int** second_quarter=find_quarter(n,int_matrix2,1);
                    matrix_multiplication(pfd[j][1],r_and_c/2,r_and_c,first_quarter,r_and_c,r_and_c/2,second_quarter);
                    free(first_quarter);
                    free(second_quarter);
                }
                /* process P3 */
                else if(i==2){
                    int** first_quarter=find_quarter(n,int_matrix1,3);
                    int** second_quarter=find_quarter(n,int_matrix2,2);
                    matrix_multiplication(pfd[j][1],r_and_c/2,r_and_c,first_quarter,r_and_c,r_and_c/2,second_quarter);
                    free(first_quarter);
                    free(second_quarter);
                }
                /* process P4 */
                else if(i==3){
                    int** first_quarter=find_quarter(n,int_matrix1,4);
                    int** second_quarter=find_quarter(n,int_matrix2,1);
                    matrix_multiplication(pfd[j][1],r_and_c/2,r_and_c,first_quarter,r_and_c,r_and_c/2,second_quarter);
                    free(first_quarter);
                    free(second_quarter);
                }
                /* process P5 */
                else if(i==4){
                    int** first_quarter=find_quarter(n,int_matrix1,4);
                    int** second_quarter=find_quarter(n,int_matrix2,2);
                    matrix_multiplication(pfd[j][1],r_and_c/2,r_and_c,first_quarter,r_and_c,r_and_c/2,second_quarter);
                    free(first_quarter);
                    free(second_quarter);
                }

                /* After the end, closes the used pipe ends */
                printf("Child %d/Process %d (PID=%ld) closing pipe \n",i,i+1,(long)getpid());
                if(close(pfd[j][1])==-1 || close(pfd[j+1][0])==-1) {
                    perror("program | main | child | close error 2");
                    exit(EXIT_FAILURE);
                }  
                free(int_matrix1);
                free(int_matrix2);
                exit(EXIT_SUCCESS);    
           
            default:
                break;        

        }
    }
    /* Parent goes on */

    /* Closes the unused pipe ends */
    for(j=0;j<8;j+=2){
        if(close(pfd[j][1])==-1 || close(pfd[j+1][0])==-1){
            perror("program | main | parent | close error");
            exit(EXIT_FAILURE);
        }
    }

    char* string_input1=malloc(r_and_c*r_and_c);
    char* string_input2=malloc(r_and_c*r_and_c);
    char* quarter1=malloc(BUFFER_SIZE);
    char* quarter2=malloc(BUFFER_SIZE);
    char* quarter3=malloc(BUFFER_SIZE);
    char* quarter4=malloc(BUFFER_SIZE);
    
    /* reads the first file from input1 and prints it */
    printf("\n**Matrix A**\n");
    if(read_lock(fd_i,string_input1,r_and_c*r_and_c)<r_and_c*r_and_c){
        perror("program | main | parent | read error or n is inconvenient");
        exit(EXIT_FAILURE);
    }
    k=0;
    for(i=0;i<r_and_c;++i){
        for(j=0;j<r_and_c;++j){
            printf("%d ",string_input1[k]);
            ++k;
        }
        printf("\n");
    }

    /* reads the second file from input1 and prints it */
    printf("\n**Matrix B**\n");
    if(read_lock(fd_j,string_input2,r_and_c*r_and_c)<r_and_c*r_and_c){
        perror("program | main | parent | read error 2 or n is inconvenient");
        exit(EXIT_FAILURE);
    }
    k=0;
    for(i=0;i<r_and_c;++i){
        for(j=0;j<r_and_c;++j){
            printf("%d ",string_input2[k]);
            ++k;
        }
        printf("\n");
    }
    printf("\n");

    /* Parent writes quarters the all pipes on write ends to send children */
    for(i=0;i<8;i+=2){
        if(write(pfd[i+1][1],string_input1,r_and_c*r_and_c)<0){
            perror("program | main | parent | write error");
            exit(EXIT_FAILURE);
        }
        if(write(pfd[i+1][1],string_input2,r_and_c*r_and_c)<0){
            perror("program | main | parent | write error 2");
            exit(EXIT_FAILURE);
        }      
    }
    
    int stat;
    for(i=0;i<4;++i){
        printf("Child (PID=%d) waited.\n",waitpid(-1,&stat,0));
    }

    /* Parent read the messages that sent by children on read ends */
    if(read(pfd[0][0],quarter1,BUFFER_SIZE)<0 || read(pfd[2][0],quarter2,BUFFER_SIZE)<0 || read(pfd[4][0],quarter3,BUFFER_SIZE)<0 ||read(pfd[6][0],quarter4,BUFFER_SIZE)<0 ){
        perror("program | main | parent | read error 3");
        exit(EXIT_FAILURE);
    }

    /* Merge them into one big matrix C */
    int **arr=merge_quarters(quarter1,quarter2,quarter3,quarter4,n);

    /* Calculates singular values and prints matrix C and the calculated values */
    calculate_singular_values(arr,n);
    

    free(string_input1);
    free(string_input2);
    free(quarter1);
    free(quarter2);
    free(quarter3);
    free(quarter4);
    free(arr);

    exit(EXIT_SUCCESS);
    
}