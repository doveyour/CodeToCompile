/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: DcharHTTPServer\r\n"

void accept_request(int);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char * ,int);
void not_found(int);
void serve_file(int, const char *,int);
int startup(u_short *);
void unimplemented(int);
void execute_post(int, const char *, const char *, const char *);
#define BURSIZE 2048

int hex2dec(char c)
{
    if ('0' <= c && c <= '9') 
    {
        return c - '0';
    } 
    else if ('a' <= c && c <= 'f')
    {
        return c - 'a' + 10;
    } 
    else if ('A' <= c && c <= 'F')
    {
        return c - 'A' + 10;
    } 
    else 
    {
        return -1;
    }
}

// 解码url
void urldecode(char url[])
{
    int i = 0;
    int len = strlen(url);
    int res_len = 0;
    char res[BURSIZE];
    for (i = 0; i < len; ++i) 
    {
        char c = url[i];
        if (c != '%') 
        {
        	if(c == '+')
        	{
			  res[res_len++] = 0x20;
			}
			else
			{
				res[res_len++] = c;
			}
        }
        else 
        {
            char c1 = url[++i];
            char c0 = url[++i];
            int num = 0;
            num = hex2dec(c1) * 16 + hex2dec(c0);
            res[res_len++] = num;
        }
    }
    res[res_len] = '\0';
    strcpy(url, res);
}
char dec2hex(short int c)
{
    if (0 <= c && c <= 9) 
    {
        return c + '0';
    } 
    else if (10 <= c && c <= 15) 
    {
        return c + 'A' - 10;
    } 
    else 
    {
        return -1;
    }
}


//编码一个url
void urlencode(char url[])
{
    int i = 0;
    int len = strlen(url);
    int res_len = 0;
    char res[BURSIZE];
    for (i = 0; i < len; ++i) 
    {
        char c = url[i];
        if (    ('0' <= c && c <= '9') ||
                ('a' <= c && c <= 'z') ||
                ('A' <= c && c <= 'Z') || 
                c == '/' || c == '.') 
        {
            res[res_len++] = c;
        } 
        else 
        {
            int j = (short int)c;
            if (j < 0)
                j += 256;
            int i1, i0;
            i1 = j / 16;
            i0 = j - i1 * 16;
            res[res_len++] = '%';
            res[res_len++] = dec2hex(i1);
            res[res_len++] = dec2hex(i0);
        }
    }
    res[res_len] = '\0';
    strcpy(url, res);
}

int readn(int fd, void *ptr, size_t n)
{
    size_t     nleft;
    ssize_t    nread;

    nleft = n;
    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (nleft == n)
                return(-1);    /* error, return -1 */
            else
                break;        /* error, return amount read so far */
        }
        else if (nread == 0)
        {
            break;            /* EOF */
        }
        nleft -= nread;
        ptr   += nread;
    }
    return(n - nleft);             /* return >= 0 */
}

int writen(int fd, const void *ptr, size_t n)
{
    size_t    nleft;
    ssize_t   nwritten;

    nleft = n;
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) < 0)
        {
            if (nleft == n)
                return(-1);    /* error, return -1 */
            else
                break;        /* error, return amount written so far */
        }
        else if (nwritten == 0)
        {
            break;            
        }
        nleft -= nwritten;
        ptr   += nwritten;
    }
    return(n - nleft);    /* return >= 0 */
}
void make_http_header(char *buf, int len)
{
	char buf1[100]	= {0};
	/*正常的 HTTP header */
	strcat(buf, "HTTP/1.1 200 OK\r\n");
	/*服务器信息*/
	strcat(buf, SERVER_STRING);
	
	strcat(buf, "Content-Type: text/html; charset=\"UTF-8\"\r\n");
	//长度
	snprintf(buf1,100,"Content-Length: %d\r\n",len);
	strcat(buf,buf1);
	
	strcat(buf, "\r\n");
}

void make_html_start(char *buf)
{
	strcat(buf, "<html>\r\n");
}


void make_html_end(char *buf)
{
	strcat(buf, "</html>\r\n");
}

void make_html_body_start(char *buf)
{
	strcat(buf, "<body>\r\n");
}
void make_html_body_end(char *buf)
{
	strcat(buf, "</body>\r\n");
}

void make_html_head(char *buf)
{
	char buf1[100]	= {0};
	strcat(buf, "<head>\r\n");
	strcat(buf, "<title>CodeToCompile</title>\r\n");
	strcat(buf, "<meta charset=\"utf-8\" />\r\n");
	strcat(buf, "</head>\r\n");
}

/**********************************************************************/
/* tran the code to compile,the len is not variety
 * return.  null
 * Parameters: orign code  tran code
 */
/**********************************************************************/

int tran_to_compile(char *orign)
{
    int i ,j; 
	char *p = NULL ;
	char *p1 = NULL ;
	if(orign == NULL)
	{
		return 0;
	}
	//buff = (char *)malloc((strlen(orign)+1)*sizeof(char));
	/*
	for(i=0;i<strlen(orign);i++)//遍历所有字符
	{
		if(orign[i] != '\0')
		{
			p1 = strstr(orign[i],"/*");
			if( NULL != p1)
			{
				memcpy(buff,orign[],)

			}
			else
			{	
				
				memcpy(buff,orign[],)
 			 	break;//没有多行注释，全部拷贝
			}



			
		}
		if(*i == '/'&&*(++i) == '*')//找到第一个/*
		{
		}
	}*/
	/*for(j=0;j<strlen(orign)+1;j++)
	{

		printf("0x%x ",orign[j]);
	}*/
	//printf("\n");
	//for(i=0;i<strlen(orign);)//遍历所有字符
	{
		i= 0;
		while(orign[i] != '\0')
		{
			//printf("--&orign[%d]:%s",i,&orign[i]);
			p = strstr(&orign[i],"&#12288;");
			if(p != NULL)
			{

				//找到之后开始修改

				for(j=0;j<8;j++)
				{
					orign[i +(p-&orign[i])+j] = 0x20;
				}
				//printf("found:%d\n",p-orign+1);
				i = i +(p-&orign[i]+1)+1;//跳过&#12288;的开头&开始找
				//break;
			}
			else //用于最后找不到字符串将指针移到末尾
			{
				i++;
			}

			/*
			for(j=0;j<strlen("&#12288;");j++)
			{
				
				orign[p-orign+j] = 0x20;

			}*/

		}
		return 1;
	}


}

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void execute_post(int client, const char *path, const char *method, const char *query_string)
{		
		char buf[1024]  = {0};
		int numchars,content_length;
		int readlen;
		char *p = NULL ;
		
		char *p2 = NULL ;
		int ret,i;
	    numchars = get_line(client, buf, sizeof(buf));
	    /*注意这里的判断条件，读到空行则继续往下读取*/
		//读取post头部数据，先获取post入参长度
        while ((numchars > 0) && strcmp("\n", buf))
        {
            /*利用 \0 进行分隔 */
            buf[15] = '\0';
            /* HTTP 请求的特点*/
            if (strcasecmp(buf, "Content-Length:") == 0)
            {
                content_length = atoi(&(buf[16]));
			}
			
            numchars = get_line(client, buf, sizeof(buf));
		}
		printf("content_length:%d\n",content_length);
		//读取入参字符串
		p = (char *)malloc((content_length+1)*sizeof(char));
		
		memset(p,0,content_length+1);
		
		readlen = readn(client,p,content_length);
		
		if(readlen == content_length)
		{	
		
			//printf("read len:%d is ok:%s \n",strlen(p),p);
			urldecode(p);
				
			//printf("urldecode:%d is ok:%s \n",strlen(p),p);
		}
		else
		{
			bad_request(client);
			printf("read is error:%s\n",p);

		}

		memset(buf,0,sizeof(buf));
		//制作http头
		make_http_header(buf,strlen(p)+220);//html代码字符220
		//发送http响应头
	    send(client, buf, strlen(buf), 0);
		
		memset(buf,0,sizeof(buf));
		make_html_start(buf);
	    send(client, buf, strlen(buf), 0);
		
		memset(buf,0,sizeof(buf));
		make_html_head(buf);
	    send(client, buf, strlen(buf), 0);


		//制作HTML body
		
		memset(buf,0,sizeof(buf));
		make_html_body_start(buf);
	    send(client, buf, strlen(buf), 0);


		memset(buf,0,sizeof(buf));
 		memcpy(buf,"<p>Transferrd Code:</p>\r\n",sizeof(buf));
	    send(client, buf, strlen(buf), 0);


		//html 文本输入框
		memset(buf,0,sizeof(buf));
 		memcpy(buf,"<textarea rows=\"38\" cols=\"256\">\r\n",sizeof(buf));
	    send(client, buf, strlen(buf), 0);

		//以下是文本处理地方
		//urlencode(p);
		/*
		for(i=0;i<100;i++)
		{
			printf("0x%d ",p[i]);
		}
		printf("\n");*/

		ret = tran_to_compile(p);
		//printf("tran_to_compile is :%s\n",p);
		if(ret == 1)
		{
			printf("send is ok:%d\n",ret);
		}
		else
		{	
			bad_request(client);
			printf("send is error:%d,%d\n",ret,content_length);
		}

		
	  	ret =  writen(client,p+4,strlen(p)-4);
		
		if(ret == strlen(p)-4)
		{
		  	printf("send is ok:%d\n",ret);
		}
		else
		{	
			bad_request(client);
			printf("send is error:%d,%d\n",ret,content_length);
		}
		

		memset(buf,0,sizeof(buf));
 		memcpy(buf,"\r\n",sizeof(buf));
	    send(client, buf, strlen(buf), 0);
			
		memset(buf,0,sizeof(buf));
 		memcpy(buf,"</textarea>\r\n",sizeof(buf));
	    send(client, buf, strlen(buf), 0);
		
		memset(buf,0,sizeof(buf));
		memcpy(buf,"<br/>",sizeof(buf));
		send(client, buf, strlen(buf), 0);

		memset(buf,0,sizeof(buf));
		memcpy(buf,"<a href=\"/index.html\">Return Home</a>\r\n",sizeof(buf));
		send(client, buf, strlen(buf), 0);
		
		memset(buf,0,sizeof(buf));
		make_html_body_end(buf);
	    send(client, buf, strlen(buf), 0);

		
		memset(buf,0,sizeof(buf));
		make_html_end(buf);
	    send(client, buf, strlen(buf), 0);

		

		//ret = writen(client,p,content_length);

		
		free(p);
		p = NULL;


	
}

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void accept_request(int client)
{
    char buf[1024];
    int numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    int cgi = 0;      /* becomes true if server decides this is a CGI program */
    char *query_string = NULL;
    char input_para[1024] = {0};
    /*得到请求的第一行*/
    numchars = get_line(client, buf, sizeof(buf));
    i = 0; j = 0;
    /*把客户端的请求方法存到 method 数组*/
	printf("buf:%s\n",buf);
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[j];
        i++; j++;
    }
    method[i] = '\0';

    /*如果既不是 GET 又不是 POST 则无法处理 */
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        unimplemented(client);
        return;
    }

    /* POST 的时候开启 cgi */
    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    /*读取 url 地址*/
    i = 0;
    while (ISspace(buf[j]) && (j < sizeof(buf)))
        j++;
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
    {
        /*存下 url */
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';

    /*处理 GET 方法*/
    if (strcasecmp(method, "GET") == 0)
    {
        /* 待处理请求为 url */
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        /* GET 方法特点，? 后面为参数*/
        if (*query_string == '?')
        {
            /*开启 cgi */
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }
	
	else if (strcasecmp(method, "POST") == 0)
	{

		query_string = url;
		if(strstr(query_string,"code"))
		{
			printf("char code bingo\n");
		}
		else
		{	
			bad_request(client);
			printf("can not find\n");

		}
	}
    /*格式化 url 到 path 数组，html 文件都在 htdocs 中*/
    sprintf(path, "htdocs%s", url);
	
    /*默认情况为 index.html */
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");
	
	printf("path:%s\n",path);
    /*根据路径找到对应文件 */
    if (stat(path, &st) == -1) 
	{
		
	    printf("path is null \n");
        /*把所有 headers 的信息都丢弃*/
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
        /*回应客户端找不到*/
        not_found(client);
    }
    else
    {
    	
	    printf(" find index path filelen:%d \n",st.st_size);
        /*如果是个目录，则默认使用该目录下 index.html 文件*/
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
    //  if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)    )
          //cgi = 0;
      /*不是 cgi,直接把服务器文件返回，否则执行 cgi */
      if (!cgi)
      {
		  printf("static process file:%s\n",path);
          serve_file(client, path,st.st_size);

	  }
      else
      {
      	
	      printf("cgi query_string:%s\n",query_string);
          execute_post(client, path, method, query_string);
	  }
    }
	printf(" close(%d)\n",client);
    /*断开与客户端的连接（HTTP 特点：无连接）*/
    close(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
    char buf[1024];

    /*回应客户端错误的 HTTP 请求 */
    sprintf(buf, "HTTP/1.1 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
    char buf[1024];

    /*读取文件中的所有数据写到 socket */
    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
    char buf[1024];

    /* 回应客户端 cgi 无法执行*/
    sprintf(buf, "HTTP/1.1 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
    /*出错信息处理 */
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
#if 0
void execute_cgi(int client, const char *path, const char *method, const char *query_string)
{
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A'; buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0)
        /*把所有的 HTTP header 读取并丢弃*/
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
    else    /* POST */
    {
        /* 对 POST 的 HTTP 请求中找出 content_length */
        numchars = get_line(client, buf, sizeof(buf));
	/*注意这里的判断条件，读到空行则继续往下读取*/
        while ((numchars > 0) && strcmp("\n", buf))
        {
            /*利用 \0 进行分隔 */
            buf[15] = '\0';
            /* HTTP 请求的特点*/
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));
            numchars = get_line(client, buf, sizeof(buf));
        }
        /*没有找到 content_length */
        if (content_length == -1) {
            /*错误请求*/
            bad_request(client);
            return;
        }
    }

    /* 正确，HTTP 状态码 200 */
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    /* 建立管道*/
    if (pipe(cgi_output) < 0) {
        /*错误处理*/
        cannot_execute(client);
        return;
    }
    /*建立管道*/
    if (pipe(cgi_input) < 0) {
        /*错误处理*/
        cannot_execute(client);
        return;
    }

    if ((pid = fork()) < 0 ) {
        /*错误处理*/
        cannot_execute(client);
        return;
    }
    if (pid == 0)  /* child: CGI script */
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        /* 把 STDOUT 重定向到 cgi_output 的写入端 */
        dup2(cgi_output[1], 1);
        /* 把 STDIN 重定向到 cgi_input 的读取端 */
        dup2(cgi_input[0], 0);
        /* 关闭 cgi_input 的写入端 和 cgi_output 的读取端 */
        close(cgi_output[0]);
        close(cgi_input[1]);
        /*设置 request_method 的环境变量*/
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);
        if (strcasecmp(method, "GET") == 0) {
            /*设置 query_string 的环境变量*/
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {   /* POST */
            /*设置 content_length 的环境变量*/
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        /*用 execl 运行 cgi 程序*/
        execl(path, path, NULL);
        exit(0);
    } else {    /* parent */
        /* 关闭 cgi_input 的读取端 和 cgi_output 的写入端 */
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(method, "POST") == 0)
            /*接收 POST 过来的数据*/
            for (i = 0; i < content_length; i++) {
                recv(client, &c, 1, 0);
                /*把 POST 数据写入 cgi_input，现在重定向到 STDIN */
                write(cgi_input[1], &c, 1);
            }
        /*读取 cgi_output 的管道输出到客户端，该管道输入是 STDOUT */
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);

        /*关闭管道*/
        close(cgi_output[0]);
        close(cgi_input[1]);
        /*等待子进程*/
        waitpid(pid, &status, 0);
    }
}
#endif
/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    /*把终止条件统一为 \n 换行符，标准化 buf 数组*/
    while ((i < size - 1) && (c != '\n'))
    {
        /*一次仅接收一个字节*/
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            /*收到 \r 则继续接收下个字节，因为换行符可能是 \r\n */
            if (c == '\r')
            {
                /*使用 MSG_PEEK 标志使下一次读取依然可以得到这次读取的内容，可认为接收窗口不滑动*/
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                /*但如果是换行符则把它吸收掉*/
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            /*存到缓冲区*/
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    /*返回 buf 数组大小*/
    return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename,int len)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    /*正常的 HTTP header */
    strcpy(buf, "HTTP/1.1 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    /*服务器信息*/
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
	//长度
	sprintf(buf, "Content-Length: %d\r\n",len);
    send(client, buf, strlen(buf), 0);
	
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
    char buf[1024];

    /* 404 页面 */
    sprintf(buf, "HTTP/1.1 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    /*服务器信息*/
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename,int filelen)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];
    int file_len = 0;
    /*读取并丢弃 header */
    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    /*打开 sever 的文件*/
    resource = fopen(filename, "r");
    if (resource == NULL)
    {

		not_found(client);
		printf("not_found index.html");
	}
    else
    {
    
		printf("found index.html");
        /*写 HTTP header */
        headers(client, filename,filelen);
        /*复制文件*/
        cat(client, resource);
    }
    fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
    int httpd = 0;
    struct sockaddr_in name;

    /*建立 socket */
    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");
    /*如果当前指定端口是 0，则动态随机分配一个端口*/
    if (*port == 0)  /* if dynamically allocating a port */
    {
        int namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
        *port = ntohs(name.sin_port);
    }
    /*开始监听*/
    if (listen(httpd, 5) < 0)
        error_die("listen");
    /*返回 socket id */
    return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{
    char buf[1024];

    /* HTTP method 不被支持*/
    sprintf(buf, "HTTP/1.1 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    /*服务器信息*/
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(int argc, char **argv)//前面是数量，后面是字符串的数组
{

    int server_sock = -1;
    u_short port ;
    int client_sock = -1;
    struct sockaddr_in client_name;
    int client_name_len = sizeof(client_name);
    pthread_t newthread;
	if (argc != 2)
	{
		//atoi();
		fprintf(stderr, "usage: %s <port> \n", argv[0]);
		exit(0);
	}
	
	port = atoi(argv[1]);

    /*在对应端口建立 httpd 服务*/
    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);


    while (1)
    {
        /*套接字收到客户端连接请求*/
        client_sock = accept(server_sock,(struct sockaddr *)&client_name,&client_name_len);
        if (client_sock == -1)
            error_die("accept");
        /*派生新线程用 accept_request 函数处理新请求*/
        /* accept_request(client_sock); */
        if (pthread_create(&newthread , NULL, accept_request, client_sock) != 0)
        {
            perror("pthread_create faild");

		}
    }

    close(server_sock);

    return(0);
}
