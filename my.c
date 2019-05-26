int RTMPPacket_Alloc(RTMPPacket *p, int nSize){
  char *ptr = calloc(1, nSize + RTMP_MAX_HEADER_SIZE);
  if (!ptr)
    return FALSE;
  p->m_body = ptr + RTMP_MAX_HEADER_SIZE;//RTMP_MAX_HEADER_SIZE 18
  p->m_nBytesRead = 0;
  return TRUE;
}

RTMP * RTMP_Alloc(){
  return calloc(1, sizeof(RTMP));
}

void RTMP_Free(RTMP *r){
  free(r);
}

void RTMP_Init(RTMP *r){
#ifdef CRYPTO
  if (!RTMP_TLS_ctx)
    RTMP_TLS_Init();
#endif

  memset(r, 0, sizeof(RTMP));
  r->m_sb.sb_socket = -1;
  r->m_inChunkSize = RTMP_DEFAULT_CHUNKSIZE;//RTMP_DEFAULT_CHUNKSIZE	128
  r->m_outChunkSize = RTMP_DEFAULT_CHUNKSIZE;
  r->m_nBufferMS = 30000;
  r->m_nClientBW = 2500000;
  r->m_nClientBW2 = 2;
  r->m_nServerBW = 2500000;
  r->m_fAudioCodecs = 3191.0;
  r->m_fVideoCodecs = 252.0;
  r->Link.timeout = 30;
  r->Link.swfAge = 30;
}

void RTMP_EnableWrite(RTMP *r){
  r->Link.protocol |= RTMP_FEATURE_WRITE;
}

void  RTMPPacket_Reset(RTMPPacket *p){
  p->m_headerType = 0;
  p->m_packetType = 0;
  p->m_nChannel = 0;
  p->m_nTimeStamp = 0;
  p->m_nInfoField2 = 0;
  p->m_hasAbsTimestamp = FALSE;
  p->m_nBodySize = 0;
  p->m_nBytesRead = 0;
}


int RTMP_SetupURL(RTMP *r, char *url){
  AVal opt, arg;
  char *p1, *p2, *ptr = strchr(url, ' ');
  int ret, len;
  unsigned int port = 0;

  if (ptr)
    *ptr = '\0';

  len = strlen(url);
  ret = RTMP_ParseURL(url, &r->Link.protocol, &r->Link.hostname,
  	&port, &r->Link.playpath0, &r->Link.app);
  if (!ret)
    return ret;
  r->Link.port = port;
  r->Link.playpath = r->Link.playpath0;

  while (ptr) {
    *ptr++ = '\0';
    p1 = ptr;
    p2 = strchr(p1, '=');
    if (!p2)
      break;
    opt.av_val = p1;
    opt.av_len = p2 - p1;
    *p2++ = '\0';
    arg.av_val = p2;
    ptr = strchr(p2, ' ');
    if (ptr) {
      *ptr = '\0';
      arg.av_len = ptr - p2;
      /* skip repeated spaces */
      while(ptr[1] == ' ')
      	*ptr++ = '\0';
    } else {
      arg.av_len = strlen(p2);
    }

    /* unescape */
    port = arg.av_len;
    for (p1=p2; port >0;) {
      if (*p1 == '\\') {
	unsigned int c;
	if (port < 3)
	  return FALSE;
	sscanf(p1+1, "%02x", &c);
	*p2++ = c;
	port -= 3;
	p1 += 3;
      } else {
	*p2++ = *p1++;
	port--;
      }
    }
    arg.av_len = p2 - arg.av_val;

    ret = RTMP_SetOpt(r, &opt, &arg);
    if (!ret)
      return ret;
  }

  if (!r->Link.tcUrl.av_len)
    {
      r->Link.tcUrl.av_val = url;
      if (r->Link.app.av_len)
        {
          if (r->Link.app.av_val < url + len)
    	    {
    	      /* if app is part of original url, just use it */
              r->Link.tcUrl.av_len = r->Link.app.av_len + (r->Link.app.av_val - url);
    	    }
    	  else
    	    {
    	      len = r->Link.hostname.av_len + r->Link.app.av_len +
    		  sizeof("rtmpte://:65535/");
	      r->Link.tcUrl.av_val = malloc(len);
	      r->Link.tcUrl.av_len = snprintf(r->Link.tcUrl.av_val, len,
		"%s://%.*s:%d/%.*s",
		RTMPProtocolStringsLower[r->Link.protocol],
		r->Link.hostname.av_len, r->Link.hostname.av_val,
		r->Link.port,
		r->Link.app.av_len, r->Link.app.av_val);
	      r->Link.lFlags |= RTMP_LF_FTCU;
	    }
        }
      else
        {
	  r->Link.tcUrl.av_len = strlen(url);
	}
    }

#ifdef CRYPTO
  if ((r->Link.lFlags & RTMP_LF_SWFV) && r->Link.swfUrl.av_len)
    RTMP_HashSWF(r->Link.swfUrl.av_val, &r->Link.SWFSize,
	  (unsigned char *)r->Link.SWFHash, r->Link.swfAge);
#endif

  SocksSetup(r, &r->Link.sockshost);

  if (r->Link.port == 0)
    {
      if (r->Link.protocol & RTMP_FEATURE_SSL)
	r->Link.port = 443;
      else if (r->Link.protocol & RTMP_FEATURE_HTTP)
	r->Link.port = 80;
      else
	r->Link.port = 1935;
    }
  return TRUE;
}



int RTMP_Connect(RTMP *r, RTMPPacket *cp){//调用时cp为空
  struct sockaddr_in service;
  if (!r->Link.hostname.av_len)
    return FALSE;

  memset(&service, 0, sizeof(struct sockaddr_in));
  service.sin_family = AF_INET;

  if (r->Link.socksport)
    {
      /* Connect via SOCKS */
      if (!add_addr_info(&service, &r->Link.sockshost, r->Link.socksport))//获取服务器server的信息
	return FALSE;
    }
  else
    {
      /* Connect directly */
      if (!add_addr_info(&service, &r->Link.hostname, r->Link.port))
	return FALSE;
    }

  if (!RTMP_Connect0(r, (struct sockaddr *)&service))//完成tcp连接
    return FALSE;

  r->m_bSendCounter = TRUE;

  return RTMP_Connect1(r, cp);//
}

static int
add_addr_info(struct sockaddr_in *service, AVal *host, int port)
{
  char *hostname;
  int ret = TRUE;
  if (host->av_val[host->av_len])
    {
      hostname = malloc(host->av_len+1);
      memcpy(hostname, host->av_val, host->av_len);
      hostname[host->av_len] = '\0';
    }
  else
    {
      hostname = host->av_val;
    }

  service->sin_addr.s_addr = inet_addr(hostname);
  if (service->sin_addr.s_addr == INADDR_NONE)
    {
      struct hostent *host = gethostbyname(hostname);
      if (host == NULL || host->h_addr == NULL)
	{
	  RTMP_Log(RTMP_LOGERROR, "Problem accessing the DNS. (addr: %s)", hostname);
	  ret = FALSE;
	  goto finish;
	}
      service->sin_addr = *(struct in_addr *)host->h_addr;
    }

  service->sin_port = htons(port);
finish:
  if (hostname != host->av_val)
    free(hostname);
  return ret;
}

int
RTMP_Connect0(RTMP *r, struct sockaddr * service)
{
  int on = 1;
  r->m_sb.sb_timedout = FALSE;
  r->m_pausing = 0;
  r->m_fDuration = 0.0;

  r->m_sb.sb_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (r->m_sb.sb_socket != -1)
    {
      if (connect(r->m_sb.sb_socket, service, sizeof(struct sockaddr)) < 0)
	{
	  int err = GetSockError();
	  RTMP_Log(RTMP_LOGERROR, "%s, failed to connect socket. %d (%s)",
	      __FUNCTION__, err, strerror(err));
	  RTMP_Close(r);
	  return FALSE;
	}

      if (r->Link.socksport)
	{
	  RTMP_Log(RTMP_LOGDEBUG, "%s ... SOCKS negotiation", __FUNCTION__);
	  if (!SocksNegotiate(r))
	    {
	      RTMP_Log(RTMP_LOGERROR, "%s, SOCKS negotiation failed.", __FUNCTION__);
	      RTMP_Close(r);
	      return FALSE;
	    }
	}
    }
  else
    {
      RTMP_Log(RTMP_LOGERROR, "%s, failed to create socket. Error: %d", __FUNCTION__,
	  GetSockError());
      return FALSE;
    }

  /* set timeout */
  {
	  /*  #define SET_RCVTIMEO(tv,s)	int tv = s*1000  */
	  /*  r->Link.timeout = 30;初始化时是30，单位是ms  */
    SET_RCVTIMEO(tv, r->Link.timeout);
    if (setsockopt
        (r->m_sb.sb_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)))
      {
        RTMP_Log(RTMP_LOGERROR, "%s, Setting socket timeout to %ds failed!",
	    __FUNCTION__, r->Link.timeout);
      }
  }
  /*TCP_NODELAY BOOL 禁止发送合并的Nagle算法。*/
  setsockopt(r->m_sb.sb_socket, IPPROTO_TCP, TCP_NODELAY, (char *) &on, sizeof(on));

  return TRUE;
}

int
RTMP_Connect1(RTMP *r, RTMPPacket *cp)
{
  if (r->Link.protocol & RTMP_FEATURE_SSL)
    {
#if defined(CRYPTO) && !defined(NO_SSL)
      TLS_client(RTMP_TLS_ctx, r->m_sb.sb_ssl);
      TLS_setfd(r->m_sb.sb_ssl, r->m_sb.sb_socket);
      if (TLS_connect(r->m_sb.sb_ssl) < 0)
	{
	  RTMP_Log(RTMP_LOGERROR, "%s, TLS_Connect failed", __FUNCTION__);
	  RTMP_Close(r);
	  return FALSE;
	}
#else
      RTMP_Log(RTMP_LOGERROR, "%s, no SSL/TLS support", __FUNCTION__);
      RTMP_Close(r);
      return FALSE;

#endif
    }
  if (r->Link.protocol & RTMP_FEATURE_HTTP)//RTMP_FEATURE_HTTP	0x01
    {
      r->m_msgCounter = 1;
      r->m_clientID.av_val = NULL;
      r->m_clientID.av_len = 0;
      HTTP_Post(r, RTMPT_OPEN, "", 1);
      if (HTTP_read(r, 1) != 0)
	{
	  r->m_msgCounter = 0;
	  RTMP_Log(RTMP_LOGDEBUG, "%s, Could not connect for handshake", __FUNCTION__);
	  RTMP_Close(r);
	  return 0;
	}
      r->m_msgCounter = 0;
    }
  RTMP_Log(RTMP_LOGDEBUG, "%s, ... connected, handshaking", __FUNCTION__);
  if (!HandShake(r, TRUE))
    {
      RTMP_Log(RTMP_LOGERROR, "%s, handshake failed.", __FUNCTION__);
      RTMP_Close(r);
      return FALSE;
    }
  RTMP_Log(RTMP_LOGDEBUG, "%s, handshaked", __FUNCTION__);

  if (!SendConnectPacket(r, cp))
    {
      RTMP_Log(RTMP_LOGERROR, "%s, RTMP connect failed.", __FUNCTION__);
      RTMP_Close(r);
      return FALSE;
    }
  return TRUE;
}

static int
HandShake(RTMP * r, int FP9HandShake)//FP9HandShake=1
{
  int i, offalg = 0;
  int dhposClient = 0;
  int digestPosClient = 0;
  int encrypted = r->Link.protocol & RTMP_FEATURE_ENC;

  RC4_handle keyIn = 0;
  RC4_handle keyOut = 0;

  int32_t *ip;
  uint32_t uptime;

  uint8_t clientbuf[RTMP_SIG_SIZE + 4], *clientsig=clientbuf+4;//RTMP_SIG_SIZE 1536
  uint8_t serversig[RTMP_SIG_SIZE], client2[RTMP_SIG_SIZE], *reply;
  uint8_t type;
  getoff *getdh = NULL, *getdig = NULL;

  if (encrypted || r->Link.SWFSize)
    FP9HandShake = TRUE;
  else
    FP9HandShake = FALSE;

  r->Link.rc4keyIn = r->Link.rc4keyOut = 0;

  if (encrypted)
    {
      clientsig[-1] = 0x06;	/* 0x08 is RTMPE as well */
      offalg = 1;
    }
  else
    clientsig[-1] = 0x03;

  uptime = htonl(RTMP_GetTime());
  memcpy(clientsig, &uptime, 4);

  if (FP9HandShake)
    {
      /* set version to at least 9.0.115.0 */
      if (encrypted)
	{
	  clientsig[4] = 128;
	  clientsig[6] = 3;
	}
      else
        {
	  clientsig[4] = 10;
	  clientsig[6] = 45;
	}
      clientsig[5] = 0;
      clientsig[7] = 2;

      RTMP_Log(RTMP_LOGDEBUG, "%s: Client type: %02X", __FUNCTION__, clientsig[-1]);
      getdig = digoff[offalg];
      getdh  = dhoff[offalg];
    }
  else
    {
      memset(&clientsig[4], 0, 4);
    }

  /* generate random data */
#ifdef _DEBUG
  memset(clientsig+8, 0, RTMP_SIG_SIZE-8);
#else
  ip = (int32_t *)(clientsig+8);
  for (i = 2; i < RTMP_SIG_SIZE/4; i++)
    *ip++ = rand();
#endif

  /* set handshake digest */
  if (FP9HandShake)
    {
      if (encrypted)
	{
	  /* generate Diffie-Hellmann parameters */
	  r->Link.dh = DHInit(1024);
	  if (!r->Link.dh)
	    {
	      RTMP_Log(RTMP_LOGERROR, "%s: Couldn't initialize Diffie-Hellmann!",
		  __FUNCTION__);
	      return FALSE;
	    }

	  dhposClient = getdh(clientsig, RTMP_SIG_SIZE);
	  RTMP_Log(RTMP_LOGDEBUG, "%s: DH pubkey position: %d", __FUNCTION__, dhposClient);

	  if (!DHGenerateKey(r->Link.dh))
	    {
	      RTMP_Log(RTMP_LOGERROR, "%s: Couldn't generate Diffie-Hellmann public key!",
		  __FUNCTION__);
	      return FALSE;
	    }

	  if (!DHGetPublicKey(r->Link.dh, &clientsig[dhposClient], 128))
	    {
	      RTMP_Log(RTMP_LOGERROR, "%s: Couldn't write public key!", __FUNCTION__);
	      return FALSE;
	    }
	}

      digestPosClient = getdig(clientsig, RTMP_SIG_SIZE);	/* reuse this value in verification */
      RTMP_Log(RTMP_LOGDEBUG, "%s: Client digest offset: %d", __FUNCTION__,
	  digestPosClient);

      CalculateDigest(digestPosClient, clientsig, GenuineFPKey, 30,
		      &clientsig[digestPosClient]);

      RTMP_Log(RTMP_LOGDEBUG, "%s: Initial client digest: ", __FUNCTION__);
      RTMP_LogHex(RTMP_LOGDEBUG, clientsig + digestPosClient,
	     SHA256_DIGEST_LENGTH);
    }

#ifdef _DEBUG
  RTMP_Log(RTMP_LOGDEBUG, "Clientsig: ");
  RTMP_LogHex(RTMP_LOGDEBUG, clientsig, RTMP_SIG_SIZE);
#endif

  if (!WriteN(r, (char *)clientsig-1, RTMP_SIG_SIZE + 1))//发送c0+c1
    return FALSE;

  if (ReadN(r, (char *)&type, 1) != 1)	/* 0x03 or 0x06 */ //接收s0
    return FALSE;

  RTMP_Log(RTMP_LOGDEBUG, "%s: Type Answer   : %02X", __FUNCTION__, type);

  if (type != clientsig[-1])
    RTMP_Log(RTMP_LOGWARNING, "%s: Type mismatch: client sent %d, server answered %d",
	__FUNCTION__, clientsig[-1], type);

  if (ReadN(r, (char *)serversig, RTMP_SIG_SIZE) != RTMP_SIG_SIZE)//接收s1
    return FALSE;

  /* decode server response */
  memcpy(&uptime, serversig, 4);
  uptime = ntohl(uptime);

  RTMP_Log(RTMP_LOGDEBUG, "%s: Server Uptime : %d", __FUNCTION__, uptime);
  RTMP_Log(RTMP_LOGDEBUG, "%s: FMS Version   : %d.%d.%d.%d", __FUNCTION__, serversig[4],
      serversig[5], serversig[6], serversig[7]);

  if (FP9HandShake && type == 3 && !serversig[4])
    FP9HandShake = FALSE;

#ifdef _DEBUG
  RTMP_Log(RTMP_LOGDEBUG, "Server signature:");
  RTMP_LogHex(RTMP_LOGDEBUG, serversig, RTMP_SIG_SIZE);
#endif

  if (FP9HandShake)
    {
      uint8_t digestResp[SHA256_DIGEST_LENGTH];
      uint8_t *signatureResp = NULL;

      /* we have to use this signature now to find the correct algorithms for getting the digest and DH positions */
      int digestPosServer = getdig(serversig, RTMP_SIG_SIZE);

      if (!VerifyDigest(digestPosServer, serversig, GenuineFMSKey, 36))
	{
	  RTMP_Log(RTMP_LOGWARNING, "Trying different position for server digest!");
	  offalg ^= 1;
	  getdig = digoff[offalg];
	  getdh  = dhoff[offalg];
	  digestPosServer = getdig(serversig, RTMP_SIG_SIZE);

	  if (!VerifyDigest(digestPosServer, serversig, GenuineFMSKey, 36))
	    {
	      RTMP_Log(RTMP_LOGERROR, "Couldn't verify the server digest");	/* continuing anyway will probably fail */
	      return FALSE;
	    }
	}

      /* generate SWFVerification token (SHA256 HMAC hash of decompressed SWF, key are the last 32 bytes of the server handshake) */
      if (r->Link.SWFSize)
	{
	  const char swfVerify[] = { 0x01, 0x01 };
	  char *vend = r->Link.SWFVerificationResponse+sizeof(r->Link.SWFVerificationResponse);

	  memcpy(r->Link.SWFVerificationResponse, swfVerify, 2);
	  AMF_EncodeInt32(&r->Link.SWFVerificationResponse[2], vend, r->Link.SWFSize);
	  AMF_EncodeInt32(&r->Link.SWFVerificationResponse[6], vend, r->Link.SWFSize);
	  HMACsha256(r->Link.SWFHash, SHA256_DIGEST_LENGTH,
		     &serversig[RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH],
		     SHA256_DIGEST_LENGTH,
		     (uint8_t *)&r->Link.SWFVerificationResponse[10]);
	}

      /* do Diffie-Hellmann Key exchange for encrypted RTMP */
      if (encrypted)
	{
	  /* compute secret key */
	  uint8_t secretKey[128] = { 0 };
	  int len, dhposServer;

	  dhposServer = getdh(serversig, RTMP_SIG_SIZE);
	  RTMP_Log(RTMP_LOGDEBUG, "%s: Server DH public key offset: %d", __FUNCTION__,
	    dhposServer);
	  len = DHComputeSharedSecretKey(r->Link.dh, &serversig[dhposServer],
	  				128, secretKey);
	  if (len < 0)
	    {
	      RTMP_Log(RTMP_LOGDEBUG, "%s: Wrong secret key position!", __FUNCTION__);
	      return FALSE;
	    }

	  RTMP_Log(RTMP_LOGDEBUG, "%s: Secret key: ", __FUNCTION__);
	  RTMP_LogHex(RTMP_LOGDEBUG, secretKey, 128);

	  InitRC4Encryption(secretKey,
			    (uint8_t *) & serversig[dhposServer],
			    (uint8_t *) & clientsig[dhposClient],
			    &keyIn, &keyOut);
	}


      reply = client2;
#ifdef _DEBUG
      memset(reply, 0xff, RTMP_SIG_SIZE);
#else
      ip = (int32_t *)reply;
      for (i = 0; i < RTMP_SIG_SIZE/4; i++)
        *ip++ = rand();
#endif
      /* calculate response now */
      signatureResp = reply+RTMP_SIG_SIZE-SHA256_DIGEST_LENGTH;

      HMACsha256(&serversig[digestPosServer], SHA256_DIGEST_LENGTH,
		 GenuineFPKey, sizeof(GenuineFPKey), digestResp);
      HMACsha256(reply, RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH, digestResp,
		 SHA256_DIGEST_LENGTH, signatureResp);

      /* some info output */
      RTMP_Log(RTMP_LOGDEBUG,
	  "%s: Calculated digest key from secure key and server digest: ",
	  __FUNCTION__);
      RTMP_LogHex(RTMP_LOGDEBUG, digestResp, SHA256_DIGEST_LENGTH);

#ifdef FP10
      if (type == 8 )
        {
	  uint8_t *dptr = digestResp;
	  uint8_t *sig = signatureResp;
	  /* encrypt signatureResp */
          for (i=0; i<SHA256_DIGEST_LENGTH; i+=8)
	    rtmpe8_sig(sig+i, sig+i, dptr[i] % 15);
        }
      else if (type == 9)
        {
	  uint8_t *dptr = digestResp;
	  uint8_t *sig = signatureResp;
	  /* encrypt signatureResp */
          for (i=0; i<SHA256_DIGEST_LENGTH; i+=8)
            rtmpe9_sig(sig+i, sig+i, dptr[i] % 15);
        }
#endif
      RTMP_Log(RTMP_LOGDEBUG, "%s: Client signature calculated:", __FUNCTION__);
      RTMP_LogHex(RTMP_LOGDEBUG, signatureResp, SHA256_DIGEST_LENGTH);
    }
  else
    {
		//reply就是serversig
		//把serversig中的时间戳取出，
      reply = serversig;
#if 0
      uptime = htonl(RTMP_GetTime());//再次获得时间，并替换serversig中的时间戳
      memcpy(reply+4, &uptime, 4);
#endif
    }

#ifdef _DEBUG
  RTMP_Log(RTMP_LOGDEBUG, "%s: Sending handshake response: ",
    __FUNCTION__);
  RTMP_LogHex(RTMP_LOGDEBUG, reply, RTMP_SIG_SIZE);
#endif
  if (!WriteN(r, (char *)reply, RTMP_SIG_SIZE))//发送c2
    return FALSE;

  /* 2nd part of handshake */
  if (ReadN(r, (char *)serversig, RTMP_SIG_SIZE) != RTMP_SIG_SIZE)//接收s2
    return FALSE;

#ifdef _DEBUG
  RTMP_Log(RTMP_LOGDEBUG, "%s: 2nd handshake: ", __FUNCTION__);
  RTMP_LogHex(RTMP_LOGDEBUG, serversig, RTMP_SIG_SIZE);
#endif

  if (FP9HandShake)
    {
      uint8_t signature[SHA256_DIGEST_LENGTH];
      uint8_t digest[SHA256_DIGEST_LENGTH];

      if (serversig[4] == 0 && serversig[5] == 0 && serversig[6] == 0
	  && serversig[7] == 0)
	{
	  RTMP_Log(RTMP_LOGDEBUG,
	      "%s: Wait, did the server just refuse signed authentication?",
	      __FUNCTION__);
	}
      RTMP_Log(RTMP_LOGDEBUG, "%s: Server sent signature:", __FUNCTION__);
      RTMP_LogHex(RTMP_LOGDEBUG, &serversig[RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH],
	     SHA256_DIGEST_LENGTH);

      /* verify server response */
      HMACsha256(&clientsig[digestPosClient], SHA256_DIGEST_LENGTH,
		 GenuineFMSKey, sizeof(GenuineFMSKey), digest);
      HMACsha256(serversig, RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH, digest,
		 SHA256_DIGEST_LENGTH, signature);

      /* show some information */
      RTMP_Log(RTMP_LOGDEBUG, "%s: Digest key: ", __FUNCTION__);
      RTMP_LogHex(RTMP_LOGDEBUG, digest, SHA256_DIGEST_LENGTH);

#ifdef FP10
      if (type == 8 )
        {
	  uint8_t *dptr = digest;
	  uint8_t *sig = signature;
	  /* encrypt signature */
          for (i=0; i<SHA256_DIGEST_LENGTH; i+=8)
	    rtmpe8_sig(sig+i, sig+i, dptr[i] % 15);
        }
      else if (type == 9)
        {
	  uint8_t *dptr = digest;
	  uint8_t *sig = signature;
	  /* encrypt signatureResp */
          for (i=0; i<SHA256_DIGEST_LENGTH; i+=8)
            rtmpe9_sig(sig+i, sig+i, dptr[i] % 15);
        }
#endif
      RTMP_Log(RTMP_LOGDEBUG, "%s: Signature calculated:", __FUNCTION__);
      RTMP_LogHex(RTMP_LOGDEBUG, signature, SHA256_DIGEST_LENGTH);
      if (memcmp
	  (signature, &serversig[RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH],
	   SHA256_DIGEST_LENGTH) != 0)
	{
	  RTMP_Log(RTMP_LOGWARNING, "%s: Server not genuine Adobe!", __FUNCTION__);
	  return FALSE;
	}
      else
	{
	  RTMP_Log(RTMP_LOGDEBUG, "%s: Genuine Adobe Flash Media Server", __FUNCTION__);
	}

      if (encrypted)
	{
	  char buff[RTMP_SIG_SIZE];
	  /* set keys for encryption from now on */
	  r->Link.rc4keyIn = keyIn;
	  r->Link.rc4keyOut = keyOut;


	  /* update the keystreams */
	  if (r->Link.rc4keyIn)
	    {
	      RC4_encrypt(r->Link.rc4keyIn, RTMP_SIG_SIZE, (uint8_t *) buff);
	    }

	  if (r->Link.rc4keyOut)
	    {
	      RC4_encrypt(r->Link.rc4keyOut, RTMP_SIG_SIZE, (uint8_t *) buff);
	    }
	}
    }
  else
    {
      if (memcmp(serversig, clientsig, RTMP_SIG_SIZE) != 0)//对比s2 和c1，如果一样则握手成功
	{
	  RTMP_Log(RTMP_LOGWARNING, "%s: client signature does not match!",
	      __FUNCTION__);
	}
    }

  RTMP_Log(RTMP_LOGDEBUG, "%s: Handshaking finished....", __FUNCTION__);
  return TRUE;
}


static int
SendConnectPacket(RTMP *r, RTMPPacket *cp)
{
  RTMPPacket packet;
  char pbuf[4096], *pend = pbuf + sizeof(pbuf);
  char *enc;

  if (cp)
    return RTMP_SendPacket(r, cp, TRUE);

  packet.m_nChannel = 0x03;	/* control channel (invoke) */
  packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
  packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_connect);
  enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
  *enc++ = AMF_OBJECT;

  enc = AMF_EncodeNamedString(enc, pend, &av_app, &r->Link.app);
  if (!enc)
    return FALSE;
  if (r->Link.protocol & RTMP_FEATURE_WRITE)
    {
      enc = AMF_EncodeNamedString(enc, pend, &av_type, &av_nonprivate);
      if (!enc)
	return FALSE;
    }
  if (r->Link.flashVer.av_len)
    {
      enc = AMF_EncodeNamedString(enc, pend, &av_flashVer, &r->Link.flashVer);
      if (!enc)
	return FALSE;
    }
  if (r->Link.swfUrl.av_len)
    {
      enc = AMF_EncodeNamedString(enc, pend, &av_swfUrl, &r->Link.swfUrl);
      if (!enc)
	return FALSE;
    }
  if (r->Link.tcUrl.av_len)
    {
      enc = AMF_EncodeNamedString(enc, pend, &av_tcUrl, &r->Link.tcUrl);
      if (!enc)
	return FALSE;
    }
  if (!(r->Link.protocol & RTMP_FEATURE_WRITE))
    {
      enc = AMF_EncodeNamedBoolean(enc, pend, &av_fpad, FALSE);
      if (!enc)
	return FALSE;
      enc = AMF_EncodeNamedNumber(enc, pend, &av_capabilities, 15.0);
      if (!enc)
	return FALSE;
      enc = AMF_EncodeNamedNumber(enc, pend, &av_audioCodecs, r->m_fAudioCodecs);
      if (!enc)
	return FALSE;
      enc = AMF_EncodeNamedNumber(enc, pend, &av_videoCodecs, r->m_fVideoCodecs);
      if (!enc)
	return FALSE;
      enc = AMF_EncodeNamedNumber(enc, pend, &av_videoFunction, 1.0);
      if (!enc)
	return FALSE;
      if (r->Link.pageUrl.av_len)
	{
	  enc = AMF_EncodeNamedString(enc, pend, &av_pageUrl, &r->Link.pageUrl);
	  if (!enc)
	    return FALSE;
	}
    }
  if (r->m_fEncoding != 0.0 || r->m_bSendEncoding)
    {	/* AMF0, AMF3 not fully supported yet */
      enc = AMF_EncodeNamedNumber(enc, pend, &av_objectEncoding, r->m_fEncoding);
      if (!enc)
	return FALSE;
    }
  if (enc + 3 >= pend)
    return FALSE;
  *enc++ = 0;
  *enc++ = 0;			/* end of object - 0x00 0x00 0x09 */
  *enc++ = AMF_OBJECT_END;

  /* add auth string */
  if (r->Link.auth.av_len)
    {
      enc = AMF_EncodeBoolean(enc, pend, r->Link.lFlags & RTMP_LF_AUTH);
      if (!enc)
	return FALSE;
      enc = AMF_EncodeString(enc, pend, &r->Link.auth);
      if (!enc)
	return FALSE;
    }
  if (r->Link.extras.o_num)
    {
      int i;
      for (i = 0; i < r->Link.extras.o_num; i++)
	{
	  enc = AMFProp_Encode(&r->Link.extras.o_props[i], enc, pend);
	  if (!enc)
	    return FALSE;
	}
    }
  packet.m_nBodySize = enc - packet.m_body;

  return RTMP_SendPacket(r, &packet, TRUE);
}


int RTMP_ConnectStream(RTMP *r, int seekTime){
  RTMPPacket packet = { 0 };

  /* seekTime was already set by SetupStream / SetupURL.
   * This is only needed by ReconnectStream.
   */
  if (seekTime > 0)
    r->Link.seekTime = seekTime;

  r->m_mediaChannel = 0;

  while (!r->m_bPlaying && RTMP_IsConnected(r) && RTMP_ReadPacket(r, &packet))
    {
      if (RTMPPacket_IsReady(&packet))
	{
	  if (!packet.m_nBodySize)
	    continue;
	  if ((packet.m_packetType == RTMP_PACKET_TYPE_AUDIO) ||
	      (packet.m_packetType == RTMP_PACKET_TYPE_VIDEO) ||
	      (packet.m_packetType == RTMP_PACKET_TYPE_INFO))
	    {
	      RTMP_Log(RTMP_LOGWARNING, "Received FLV packet before play()! Ignoring.");
	      RTMPPacket_Free(&packet);
	      continue;
	    }

	  RTMP_ClientPacket(r, &packet);
	  RTMPPacket_Free(&packet);
	}
    }

  return r->m_bPlaying;
}

int RTMP_SendPacket(RTMP *r, RTMPPacket *packet, int queue){
  const RTMPPacket *prevPacket;
  uint32_t last = 0;
  int nSize;
  int hSize, cSize;
  char *header, *hptr, *hend, hbuf[RTMP_MAX_HEADER_SIZE], c;
  uint32_t t;
  char *buffer, *tbuf = NULL, *toff = NULL;
  int nChunkSize;
  int tlen;

  if (packet->m_nChannel >= r->m_channelsAllocatedOut)
    {
      int n = packet->m_nChannel + 10;
      RTMPPacket **packets = realloc(r->m_vecChannelsOut, sizeof(RTMPPacket*) * n);
      if (!packets) {
        free(r->m_vecChannelsOut);
        r->m_vecChannelsOut = NULL;
        r->m_channelsAllocatedOut = 0;
        return FALSE;
      }
      r->m_vecChannelsOut = packets;
      memset(r->m_vecChannelsOut + r->m_channelsAllocatedOut, 0, sizeof(RTMPPacket*) * (n - r->m_channelsAllocatedOut));
      r->m_channelsAllocatedOut = n;
    }

  prevPacket = r->m_vecChannelsOut[packet->m_nChannel];
  if (prevPacket && packet->m_headerType != RTMP_PACKET_SIZE_LARGE)
    {
      /* compress a bit by using the prev packet's attributes */
      if (prevPacket->m_nBodySize == packet->m_nBodySize
	  && prevPacket->m_packetType == packet->m_packetType
	  && packet->m_headerType == RTMP_PACKET_SIZE_MEDIUM)
	packet->m_headerType = RTMP_PACKET_SIZE_SMALL;

      if (prevPacket->m_nTimeStamp == packet->m_nTimeStamp
	  && packet->m_headerType == RTMP_PACKET_SIZE_SMALL)
	packet->m_headerType = RTMP_PACKET_SIZE_MINIMUM;
      last = prevPacket->m_nTimeStamp;
    }

  if (packet->m_headerType > 3)	/* sanity */
    {
      RTMP_Log(RTMP_LOGERROR, "sanity failed!! trying to send header of type: 0x%02x.",
	  (unsigned char)packet->m_headerType);
      return FALSE;
    }

  nSize = packetSize[packet->m_headerType];
  hSize = nSize; cSize = 0;
  t = packet->m_nTimeStamp - last;

  if (packet->m_body)
    {
      header = packet->m_body - nSize;
      hend = packet->m_body;
    }
  else
    {
      header = hbuf + 6;
      hend = hbuf + sizeof(hbuf);
    }

  if (packet->m_nChannel > 319)
    cSize = 2;
  else if (packet->m_nChannel > 63)
    cSize = 1;
  if (cSize)
    {
      header -= cSize;
      hSize += cSize;
    }

  if (nSize > 1 && t >= 0xffffff)
    {
      header -= 4;
      hSize += 4;
    }

  hptr = header;
  c = packet->m_headerType << 6;
  switch (cSize)
    {
    case 0:
      c |= packet->m_nChannel;
      break;
    case 1:
      break;
    case 2:
      c |= 1;
      break;
    }
  *hptr++ = c;
  if (cSize)
    {
      int tmp = packet->m_nChannel - 64;
      *hptr++ = tmp & 0xff;
      if (cSize == 2)
	*hptr++ = tmp >> 8;
    }

  if (nSize > 1)
    {
      hptr = AMF_EncodeInt24(hptr, hend, t > 0xffffff ? 0xffffff : t);
    }

  if (nSize > 4)
    {
      hptr = AMF_EncodeInt24(hptr, hend, packet->m_nBodySize);
      *hptr++ = packet->m_packetType;
    }

  if (nSize > 8)
    hptr += EncodeInt32LE(hptr, packet->m_nInfoField2);

  if (nSize > 1 && t >= 0xffffff)
    hptr = AMF_EncodeInt32(hptr, hend, t);

  nSize = packet->m_nBodySize;
  buffer = packet->m_body;
  nChunkSize = r->m_outChunkSize;

  RTMP_Log(RTMP_LOGDEBUG2, "%s: fd=%d, size=%d", __FUNCTION__, r->m_sb.sb_socket,
      nSize);
  /* send all chunks in one HTTP request */
  if (r->Link.protocol & RTMP_FEATURE_HTTP)
    {
      int chunks = (nSize+nChunkSize-1) / nChunkSize;
      if (chunks > 1)
        {
	  tlen = chunks * (cSize + 1) + nSize + hSize;
	  tbuf = malloc(tlen);
	  if (!tbuf)
	    return FALSE;
	  toff = tbuf;
	}
    }
  while (nSize + hSize)
    {
      int wrote;

      if (nSize < nChunkSize)
	nChunkSize = nSize;

      RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)header, hSize);
      RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)buffer, nChunkSize);
      if (tbuf)
        {
	  memcpy(toff, header, nChunkSize + hSize);
	  toff += nChunkSize + hSize;
	}
      else
        {
	  wrote = WriteN(r, header, nChunkSize + hSize);
	  if (!wrote)
	    return FALSE;
	}
      nSize -= nChunkSize;
      buffer += nChunkSize;
      hSize = 0;

      if (nSize > 0)
	{
	  header = buffer - 1;
	  hSize = 1;
	  if (cSize)
	    {
	      header -= cSize;
	      hSize += cSize;
	    }
	  *header = (0xc0 | c);
	  if (cSize)
	    {
	      int tmp = packet->m_nChannel - 64;
	      header[1] = tmp & 0xff;
	      if (cSize == 2)
		header[2] = tmp >> 8;
	    }
	}
    }
  if (tbuf)
    {
      int wrote = WriteN(r, tbuf, toff-tbuf);
      free(tbuf);
      tbuf = NULL;
      if (!wrote)
        return FALSE;
    }

  /* we invoked a remote method */
  if (packet->m_packetType == RTMP_PACKET_TYPE_INVOKE)
    {
      AVal method;
      char *ptr;
      ptr = packet->m_body + 1;
      AMF_DecodeString(ptr, &method);
      RTMP_Log(RTMP_LOGDEBUG, "Invoking %s", method.av_val);
      /* keep it in call queue till result arrives */
      if (queue) {
        int txn;
        ptr += 3 + method.av_len;
        txn = (int)AMF_DecodeNumber(ptr);
	AV_queue(&r->m_methodCalls, &r->m_numCalls, &method, txn);
      }
    }

  if (!r->m_vecChannelsOut[packet->m_nChannel])
    r->m_vecChannelsOut[packet->m_nChannel] = malloc(sizeof(RTMPPacket));
  memcpy(r->m_vecChannelsOut[packet->m_nChannel], packet, sizeof(RTMPPacket));
  return TRUE;
}
