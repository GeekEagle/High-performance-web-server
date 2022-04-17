if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0){
    logger(ERROR,"system call","bind",0); 
    printf("hahahaha\n");
  }
  if( listen(listenfd,64) <0) 
    logger(ERROR,"system call","listen",0); 
  for(hit=1; ;hit++) { 
    length = sizeof(cli_addr); 
    if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0) 
      logger(ERROR,"system call","accept",0); 
    pid_t pid=fork();
    if (pid==0) //child process 
    {
      gettimeofday(&ts,NULL);
      web(socketfd,hit);
      gettimeofday(&te,NULL); 
      printf("current pid is:%d,its parent process is %d\n",getpid(),getppid());
      timespent = (te.tv_sec-ts.tv_sec)*1000000+te.tv_usec-ts.tv_usec;
     
      exit(0);
    }
    else if (pid > 0) // parent process 
    {
      	close(socketfd); 
    }
    else{
      perror("create childProcess error"); 
      exit(1);
    }  
  }