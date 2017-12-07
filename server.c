// Program ini dijalankan di server, yang mana
// dalam hal ini kita menggunakan Ubuntu Server 16.04 LTS
// di alamat 103.43.44.105 (http://chatmote.com)

// Melampirkan library-library yang dibutuhkan untuk
// socket programming dan multi-threading.
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<pthread.h>     // (link with -lpthread)

// Melampirkan library yang dibutuhkan untuk
// terhubung dengan database MySQL yang terinstall
// pada server. Hal ini karena pesan-pesan yang
// ditulis oleh pengguna disimpan di database ini.
#include <my_global.h>
#include <mysql.h>
 
// Digunakan untuk multi-threading (pthread)
void *connection_handler(void *);

// Digunakan apabila pada proses yang berhubungan
// dengan MySQL terdapat eror.
void finish_with_error(MYSQL *con);

// Program utama yang fungsinya adalah untuk
// menerima pesan dari pengguna, dan menyimpannya
// di database MySQL.
int main(int argc , char *argv[]) {
    // Adalah variabel dalam bagian socket programming
    int socket_desc , client_sock , c , *new_sock;
    struct sockaddr_in server , client;
     
    // Membuat soket baru, dan memberikan kabar
    // perihal apakah hal tersebut sukses dilakukan
    // atau tidak.
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
    } puts("Socket created");
    
    // Untuk socket programming
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;

    // Pada port 8888 lah program server ini akan
    // mendengarkan (listen) request dari client
    // (komputer pengguna). Atau dalam kata lain,
    // apabila ada request yang masuk ke server ini
    // pada port 8888, maka program inilah yang akan
    // meresponnya.
    server.sin_port = htons(8888);
     
    // Mulai mendengarkan (listen) request yang masuk ke program ini,
    // dan mengabarkan apakah hal tersebut sukses dilakukan atau tidak.
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0) {
        perror("bind failed. Error");
        return 1;
    } puts("bind done");
    listen(socket_desc, 3);
     
    // Mengabarkan bahwa program ini sudah siap dan sedang 
    // menunggu request dari komputer pengguna
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);

    // Bagian ini akan dijalankan jika program menerima request dari
    // komputer pengguna.
    while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ) {
        puts("Connection accepted");

        // Untuk pthread
        pthread_t sniffer_thread;

        // Untuk socket programming
        new_sock = malloc(1);
        *new_sock = client_sock;
         
        // Implementasi pthread sehingga request yang diterima selanjutnya akan
        // akan diproses pada thread lain (thread baru).
        if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0) {
            perror("could not create thread");
            return 1;
        }
        
        puts("Handler assigned");  
    }
    
    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }
     
    return 0;
}


// Implementasi pthread sehingga program tidak perlu
// menunggu satu request selesai dahulu untuk memproses
// request lain dari client lain.
void *connection_handler(void *socket_desc) {
    // Untuk socket programming
    int sock = *(int*)socket_desc;

    // Nantinya variabel ini akan berisi ukuran dari
    // request yang diterima dari pengguna
    int read_size;

    // Deklarasi variabel yang digunakan untuk menyimpan
    // request dari pengguna.
    char *message, client_message[2000];

    // Balasan dari request yang dilakukan oleh pengguna
    char verify[] = "Pesan diterima";

    // Ini adalah perintah yang nantinya akan dikirim
    // ke database MySQL, database akan merespon dari
    // apapun yang kita kirimkan kepadanya.
    // Query adalah cara program ini berkomunikasi dengan
    // database MySQL.
    char *query = malloc(64);
  
    // Apabila ada request dari pengguna
    while( (read_size = recv(sock, client_message , 2000 , 0)) > 0 ) {
    	// Menginisiasi sambungan MySQL
    	MYSQL *con = mysql_init(NULL);
      
        // Program akan berhenti di sini jika terdapat
        // eror saat menginisaisi MySQL.
      	if (con == NULL) {
    	      fprintf(stderr, "%s\n", mysql_error(con));
    	      exit(1);
    	}  

        // Melakukan koneksi dengan database MySQL
        if (mysql_real_connect(con, "localhost", "admin", "adminpass", "alpro", 0, NULL, 0) == NULL) {
            finish_with_error(con);
        }   
    	
        // Mengisi variabel 'query' dengan query yang akan dikirimkan ke MySQL.
        // Maksud dari query ini ialah untuk membuat baris baru pada tabel 'msg',
        // yang mana pada kolom 'Pesan' akan dimasukan pesan dari pengguna.
    	sprintf(query,"INSERT INTO `msg`(`Pesan`) VALUES(\"%s\")", client_message);

        // Mengirimkan query tersebut ke MySQL
    	if (mysql_query(con, query)) {
    	      finish_with_error(con);
    	} 
    	
        // Saat program sudah selesai mengirim query ke MySQL yang mana
        // adalah untuk menyimpan pesan pengguna di database, maka program
        // menghapus isi dari 'client_message'.
    	memset(client_message, 0, sizeof client_message);

        // Menutup koneksi dengan MySQL karena sudah selesai
        // urusannya dengan database.
    	mysql_close(con);    

    	// Mengirimkan respon ke pengguna bahwa request sudah diterima
        // dan diproses.
        write(sock, verify, strlen(verify));
    }

    // Memberi kabar apabila koneksi kepada pengguna
    // sudah selesai
    if(read_size == 0) {
        puts("Client disconnected");
        fflush(stdout);
    }

    else if(read_size == -1) {
        perror("recv failed");
    }
         
    // Membebaskan alokasi memori untuk
    // socket programming
    free(socket_desc);
    
    // Menutup koneksi soket
	close(sock);
    return 0;
}


// Fungsi ini akan dijalankan apabila 
// pada proses yang berkaitan dengan MySQL
// terdapat error.
void finish_with_error(MYSQL *con) {
  fprintf(stderr, "%s\n", mysql_error(con));
  mysql_close(con);
  exit(1);        
}