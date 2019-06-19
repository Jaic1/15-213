/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 

/*
 * author: chj
 * email: 506933131@qq.com
 */

#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int ii,jj;
    int i,j;
    int a0,a1,a2,a3,a4,a5,a6,a7;
    if(M == 32 && N == 32){
        for(ii = 0;ii < N;ii += 8){
            for(jj = 0;jj < M;jj += 8){
                for(i = ii;i < N && i < ii+8;i++){
                    j = jj;
                    a0 = A[i][j];
                    a1 = A[i][j+1];
                    a2 = A[i][j+2];
                    a3 = A[i][j+3];
                    a4 = A[i][j+4];
                    a5 = A[i][j+5];
                    a6 = A[i][j+6];
                    a7 = A[i][j+7];
                    B[j][i] = a0;
                    B[j+1][i] = a1;
                    B[j+2][i] = a2;
                    B[j+3][i] = a3;
                    B[j+4][i] = a4;
                    B[j+5][i] = a5;
                    B[j+6][i] = a6;
                    B[j+7][i] = a7;
                }
            }
        }
        return;
    }
    if(M == 64 && N == 64){
        /* version 1: use 8x8 block with a little optimization for the loop on columns of B (misses: 2500+)
        for(ii = 0;ii < N;ii += 8){
            for(jj = 0;jj < M;jj += 8){
                for(i = ii;i < N && i < ii+8;i++){
                    j = jj;
                    a0 = A[i][j];
                    a1 = A[i][j+1];
                    a2 = A[i][j+2];
                    a3 = A[i][j+3];
                    a4 = A[i][j+4];
                    a5 = A[i][j+5];
                    a6 = A[i][j+6];
                    a7 = A[i][j+7];
                    if((i-ii)%2 == 0){
                        B[j][i] = a0;
                        B[j+1][i] = a1;
                        B[j+2][i] = a2;
                        B[j+3][i] = a3;
                        B[j+4][i] = a4;
                        B[j+5][i] = a5;
                        B[j+6][i] = a6;
                        B[j+7][i] = a7;
                    }
                    else{
                        B[j+7][i] = a7;
                        B[j+6][i] = a6;
                        B[j+5][i] = a5;
                        B[j+4][i] = a4;
                        B[j+3][i] = a3;
                        B[j+2][i] = a2;
                        B[j+1][i] = a1;
                        B[j][i] = a0;
                    }
                }
            }
        }
        */

        /* version 2: use 4x4 block (misses: 1699)
        for(ii = 0;ii < N;ii += 4){
            for(jj = 0;jj < M;jj += 4){
                for(i = ii;i < N && i < ii+4;i++){
                    j = jj;
                    a0 = A[i][j];
                    a1 = A[i][j+1];
                    a2 = A[i][j+2];
                    a3 = A[i][j+3];
                    B[j][i] = a0;
                    B[j+1][i] = a1;
                    B[j+2][i] = a2;
                    B[j+3][i] = a3;
                }
            }
        }
        */

        /* version 3: use 4x4 block but change the inner order a little bit (misses: 1451)
        for(ii = 0;ii < N;ii += 8){
            for(jj = 0;jj < M;jj += 8){
                j = jj;
                for(i = ii;i < N && i < ii+4;i++){
                    a0 = A[i][j];
                    a1 = A[i][j+1];
                    a2 = A[i][j+2];
                    a3 = A[i][j+3];
                    B[j][i] = a0;
                    B[j+1][i] = a1;
                    B[j+2][i] = a2;
                    B[j+3][i] = a3;
                }
                j = jj + 4;
                for(i = ii;i < N && i < ii+4;i++){
                    a0 = A[i][j];
                    a1 = A[i][j+1];
                    a2 = A[i][j+2];
                    a3 = A[i][j+3];
                    B[j][i] = a0;
                    B[j+1][i] = a1;
                    B[j+2][i] = a2;
                    B[j+3][i] = a3;
                }
                j = jj + 4;
                for(i = ii+4;i < N && i < ii+8;i++){
                    a0 = A[i][j];
                    a1 = A[i][j+1];
                    a2 = A[i][j+2];
                    a3 = A[i][j+3];
                    B[j][i] = a0;
                    B[j+1][i] = a1;
                    B[j+2][i] = a2;
                    B[j+3][i] = a3;
                }
                j = jj;
                for(i = ii+4;i < N && i < ii+8;i++){
                    a0 = A[i][j];
                    a1 = A[i][j+1];
                    a2 = A[i][j+2];
                    a3 = A[i][j+3];
                    B[j][i] = a0;
                    B[j+1][i] = a1;
                    B[j+2][i] = a2;
                    B[j+3][i] = a3;
                }
            }
        }
        */

        /* version 4: use 4x4 block but change the inner and outer order a little bit (misses: 1451, nothing different from version 3) 
        for(jj = 0;jj < M;jj += 8){
            for(ii = 0;ii < N;ii += 8){
                j = jj;
                for(i = ii;i < N && i < ii+4;i++){
                    a0 = A[i][j];
                    a1 = A[i][j+1];
                    a2 = A[i][j+2];
                    a3 = A[i][j+3];
                    B[j][i] = a0;
                    B[j+1][i] = a1;
                    B[j+2][i] = a2;
                    B[j+3][i] = a3;
                }
                j = jj + 4;
                for(i = ii;i < N && i < ii+4;i++){
                    a0 = A[i][j];
                    a1 = A[i][j+1];
                    a2 = A[i][j+2];
                    a3 = A[i][j+3];
                    B[j][i] = a0;
                    B[j+1][i] = a1;
                    B[j+2][i] = a2;
                    B[j+3][i] = a3;
                }
                j = jj + 4;
                for(i = ii+4;i < N && i < ii+8;i++){
                    a0 = A[i][j];
                    a1 = A[i][j+1];
                    a2 = A[i][j+2];
                    a3 = A[i][j+3];
                    B[j][i] = a0;
                    B[j+1][i] = a1;
                    B[j+2][i] = a2;
                    B[j+3][i] = a3;
                }
                j = jj;
                for(i = ii+4;i < N && i < ii+8;i++){
                    a0 = A[i][j];
                    a1 = A[i][j+1];
                    a2 = A[i][j+2];
                    a3 = A[i][j+3];
                    B[j][i] = a0;
                    B[j+1][i] = a1;
                    B[j+2][i] = a2;
                    B[j+3][i] = a3;
                }
            }
        }
        */

        /* version 5: use 8x4 block (1651)
        for(ii = 0;ii < N;ii += 8){
            for(jj = 0;jj < M;jj += 4){
                j = jj;
                for(i = ii;i < N && i < ii+8;i++){
                    a0 = A[i][j];
                    a1 = A[i][j+1];
                    a2 = A[i][j+2];
                    a3 = A[i][j+3];
                    B[j][i] = a0;
                    B[j+1][i] = a1;
                    B[j+2][i] = a2;
                    B[j+3][i] = a3;
                }
            }
        }
        */

        /* version 6: https://blog.csdn.net/xbb224007/article/details/81103995
           use some warmed-up cache to store some data that is warmed up previously but can't be used immediately */
        for(ii = 0;ii < N;ii += 8){
            for(jj = 0;jj < M;jj += 8){
                j = jj;
                for(i = ii;i < ii+4;i++){
                    a0 = A[i][j];a1 = A[i][j+1];a2 = A[i][j+2];a3 = A[i][j+3];
                    a4 = A[i][j+4];a5 = A[i][j+5];a6 = A[i][j+6];a7 = A[i][j+7]; // fetch it and store in some area of B, which definitely will be used. So in this way, these preserved A data in B can help the misses to be used effectively.
                    
                    B[j][i] = a0;B[j+1][i] = a1;B[j+2][i] = a2;B[j+3][i] = a3;
                    B[j][i+4] = a4;B[j+1][i+4] = a5;B[j+2][i+4] = a6;B[j+3][i+4] = a7; // where the preserved A data is stored
                }
                i = ii;
                for(j = jj;j < jj+4;j++){
                    a0 = A[i+4][j];a1 = A[i+5][j];a2 = A[i+6][j];a3 = A[i+7][j]; // transpose the lower-left of A to upper-right of B
                    a4 = B[j][i+4];a5 = B[j][i+5];a6 = B[j][i+6];a7 = B[j][i+7]; // fetch the preserved data in upper-right of B

                    B[j][i+4] = a0;B[j][i+5] = a1;B[j][i+6] = a2;B[j][i+7] = a3;
                    B[j+4][i] = a4;B[j+4][i+1] = a5;B[j+4][i+2] = a6;B[j+4][i+3] = a7; // ultimately reach the lower-left of B row by row (4 int per row)
                }
                j = jj+4;
                for(i = ii+4;i < ii+8;i++){
                    a0 = A[i][j];a1 = A[i][j+1];a2 = A[i][j+2];a3 = A[i][j+3]; 
                    B[j][i] = a0;B[j+1][i] = a1;B[j+2][i] = a2;B[j+3][i] = a3;
                }
            }
        }
        return;
    }

    if(M == 61 && N == 67){
        for(ii = 0;ii < N;ii += 8){
            for(jj = 0;jj < M;jj += 8){
                j = jj;
                for(i = ii;i < N && i < ii+8;i++){
                    if(j + 8 < M){
                        a0 = A[i][j];
                        a1 = A[i][j+1];
                        a2 = A[i][j+2];
                        a3 = A[i][j+3];
                        a4 = A[i][j+4];
                        a5 = A[i][j+5];
                        a6 = A[i][j+6];
                        a7 = A[i][j+7];
                        B[j][i] = a0;
                        B[j+1][i] = a1;
                        B[j+2][i] = a2;
                        B[j+3][i] = a3;
                        B[j+4][i] = a4;
                        B[j+5][i] = a5;
                        B[j+6][i] = a6;
                        B[j+7][i] = a7;
                    }
                    else{
                        a0 = A[i][j];
                        a1 = A[i][j+1];
                        a2 = A[i][j+2];
                        a3 = A[i][j+3];
                        a4 = A[i][j+4];
                        B[j][i] = a0;
                        B[j+1][i] = a1;
                        B[j+2][i] = a2;
                        B[j+3][i] = a3;
                        B[j+4][i] = a4;
                    }
                }
            }
        }
        return;
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

