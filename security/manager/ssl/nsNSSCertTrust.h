/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSNSSCERTTRUST_H_
#define _NSNSSCERTTRUST_H_

#include "certt.h"
#include "certdb.h"

/*
 * nsNSSCertTrust
 * 
 * Class for maintaining trust flags for an NSS certificate.
 */
class nsNSSCertTrust
{
public:
  nsNSSCertTrust();
  nsNSSCertTrust(unsigned int ssl, unsigned int email, unsigned int objsign);
  explicit nsNSSCertTrust(CERTCertTrust *t);
  virtual ~nsNSSCertTrust();

  /* query */
  bool HasAnyCA();
  bool HasAnyUser();
  bool HasCA(bool checkSSL = true, 
               bool checkEmail = true,  
               bool checkObjSign = true);
  bool HasPeer(bool checkSSL = true, 
                 bool checkEmail = true,  
                 bool checkObjSign = true);
  bool HasUser(bool checkSSL = true, 
                 bool checkEmail = true,  
                 bool checkObjSign = true);
  bool HasTrustedCA(bool checkSSL = true, 
                      bool checkEmail = true,  
                      bool checkObjSign = true);
  bool HasTrustedPeer(bool checkSSL = true, 
                        bool checkEmail = true,  
                        bool checkObjSign = true);

  /* common defaults */
  /* equivalent to "c,c,c" */
  void SetValidCA();
  /* equivalent to "C,C,C" */
  void SetTrustedServerCA();
  /* equivalent to "CT,CT,CT" */
  void SetTrustedCA();
  /* equivalent to "p,," */
  void SetValidServerPeer();
  /* equivalent to "p,p,p" */
  void SetValidPeer();
  /* equivalent to "P,P,P" */
  void SetTrustedPeer();
  /* equivalent to "u,u,u" */
  void SetUser();

  /* general setters */
  /* read: "p, P, c, C, T, u, w" */
  void SetSSLTrust(bool peer, bool tPeer,
                   bool ca,   bool tCA, bool tClientCA,
                   bool user, bool warn); 

  void SetEmailTrust(bool peer, bool tPeer,
                     bool ca,   bool tCA, bool tClientCA,
                     bool user, bool warn);

  void SetObjSignTrust(bool peer, bool tPeer,
                       bool ca,   bool tCA, bool tClientCA,
                       bool user, bool warn);

  /* set c <--> CT */
  void AddCATrust(bool ssl, bool email, bool objSign);
  /* set p <--> P */
  void AddPeerTrust(bool ssl, bool email, bool objSign);

  /* get it (const?) (shallow?) */
  CERTCertTrust * GetTrust() { return &mTrust; }

private:
  void addTrust(unsigned int *t, unsigned int v);
  void removeTrust(unsigned int *t, unsigned int v);
  bool hasTrust(unsigned int t, unsigned int v);
  CERTCertTrust mTrust;
};

#endif
