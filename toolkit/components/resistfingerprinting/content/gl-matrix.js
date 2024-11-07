/*!
@fileoverview gl-matrix - High performance matrix and vector operations
@author Brandon Jones
@author Colin MacKenzie IV
@version 2.7.0

Copyright (c) 2015-2018, Brandon Jones, Colin MacKenzie IV.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/
// Tree-shaked version of gl-matrix. Only includes create, perspective, translate, and rotate functions.
(()=>{var $=Object.create;var N=Object.defineProperty;var d=Object.getOwnPropertyDescriptor;var a=Object.getOwnPropertyNames;var t=Object.getPrototypeOf,o=Object.prototype.hasOwnProperty;var E=(r,e)=>()=>(e||r((e={exports:{}}).exports,e),e.exports);var u=(r,e,c,s)=>{if(e&&typeof e=="object"||typeof e=="function")for(let n of a(e))!o.call(r,n)&&n!==c&&N(r,n,{get:()=>e[n],enumerable:!(s=d(e,n))||s.enumerable});return r};var L=(r,e,c)=>(c=r!=null?$(t(r)):{},u(e||!r||!r.__esModule?N(c,"default",{value:r,enumerable:!0}):c,r));var P=E((c1,O)=>{O.exports=r1;function r1(){var r=new Float32Array(16);return r[0]=1,r[1]=0,r[2]=0,r[3]=0,r[4]=0,r[5]=1,r[6]=0,r[7]=0,r[8]=0,r[9]=0,r[10]=1,r[11]=0,r[12]=0,r[13]=0,r[14]=0,r[15]=1,r}});var R=E((m1,Q)=>{Q.exports=e1;function e1(r,e,c,s,n){var p=1/Math.tan(e/2),i=1/(s-n);return r[0]=p/c,r[1]=0,r[2]=0,r[3]=0,r[4]=0,r[5]=p,r[6]=0,r[7]=0,r[8]=0,r[9]=0,r[10]=(n+s)*i,r[11]=-1,r[12]=0,r[13]=0,r[14]=2*n*s*i,r[15]=0,r}});var T=E((f1,S)=>{S.exports=n1;function n1(r,e,c){var s=c[0],n=c[1],p=c[2],i,b,f,l,m,h,v,x,M,y,z,q;return e===r?(r[12]=e[0]*s+e[4]*n+e[8]*p+e[12],r[13]=e[1]*s+e[5]*n+e[9]*p+e[13],r[14]=e[2]*s+e[6]*n+e[10]*p+e[14],r[15]=e[3]*s+e[7]*n+e[11]*p+e[15]):(i=e[0],b=e[1],f=e[2],l=e[3],m=e[4],h=e[5],v=e[6],x=e[7],M=e[8],y=e[9],z=e[10],q=e[11],r[0]=i,r[1]=b,r[2]=f,r[3]=l,r[4]=m,r[5]=h,r[6]=v,r[7]=x,r[8]=M,r[9]=y,r[10]=z,r[11]=q,r[12]=i*s+m*n+M*p+e[12],r[13]=b*s+h*n+y*p+e[13],r[14]=f*s+v*n+z*p+e[14],r[15]=l*s+x*n+q*p+e[15]),r}});var V=E((b1,U)=>{U.exports=p1;function p1(r,e,c,s){var n=s[0],p=s[1],i=s[2],b=Math.sqrt(n*n+p*p+i*i),f,l,m,h,v,x,M,y,z,q,G,H,I,J,K,w,A,F,g,j,k,B,C,D;return Math.abs(b)<1e-6?null:(b=1/b,n*=b,p*=b,i*=b,f=Math.sin(c),l=Math.cos(c),m=1-l,h=e[0],v=e[1],x=e[2],M=e[3],y=e[4],z=e[5],q=e[6],G=e[7],H=e[8],I=e[9],J=e[10],K=e[11],w=n*n*m+l,A=p*n*m+i*f,F=i*n*m-p*f,g=n*p*m-i*f,j=p*p*m+l,k=i*p*m+n*f,B=n*i*m+p*f,C=p*i*m-n*f,D=i*i*m+l,r[0]=h*w+y*A+H*F,r[1]=v*w+z*A+I*F,r[2]=x*w+q*A+J*F,r[3]=M*w+G*A+K*F,r[4]=h*g+y*j+H*k,r[5]=v*g+z*j+I*k,r[6]=x*g+q*j+J*k,r[7]=M*g+G*j+K*k,r[8]=h*B+y*C+H*D,r[9]=v*B+z*C+I*D,r[10]=x*B+q*C+J*D,r[11]=M*B+G*C+K*D,e!==r&&(r[12]=e[12],r[13]=e[13],r[14]=e[14],r[15]=e[15]),r)}});window.mat4=E((l1,_)=>{var W=L(P()),X=L(R()),Y=L(T()),Z=L(V());_.exports={create:W.default,perspective:X.default,translate:Y.default,rotate:Z.default}})()})();