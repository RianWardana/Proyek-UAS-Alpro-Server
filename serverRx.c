// Program ini dijalankan di server, yang mana
// dalam hal ini kita menggunakan Ubuntu Server 16.04 LTS
// di alamat 103.43.44.105 (http://chatmote.com)

// Melampirkan library-library yang dibutuhkan untuk
// socket programming.
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>

// Untuk akses ke database MYSQL
#include <my_global.h>
#include <mysql.h>

// Untuk fitur multithreading (-lpthread)
#include<pthread.h>
void *connection_handler(void *);

// Fungsi ini akan dijalankan apabila 
// pada proses yang berkaitan dengan MySQL
// terdapat error.
void finish_with_error(MYSQL *con) {
    fprintf(stderr, "%s\n", mysql_error(con));
    mysql_close(con);
    exit(1);        
}

// Ini adalah fungsi utama yang tugasnya adalah untuk
// mengirimkan pesan dari database MySQL ke pengguna.
// Pesan mana yang diambil dari database MySQL dan
// dikirim ke pengguna itu tergantung dari nomor (ID)
// pesan yang diminta
int main(int argc, char *argv[]) {
    // Untuk keperluan socket programming
    int socket_desc, client_sock, c, *new_sock;
    struct sockaddr_in server, client;
     
    // Membuat socket untuk komunikasi dengan pengguna
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
    } puts("Socket created");
     
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;

    // Program ini akan mendengarkan (listen) port
    // 8080.
    server.sin_port = htons(8080);
     
    // Membuat program mendengarkan (listen) request yang disampaikan oleh
    // pengguna, dan mengabarkan apakah hal tersebut dilakukan dengan sukses
    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind failed. Error");
        return 1;
    } puts("bind done");
    listen(socket_desc , 3);
     
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
        if( pthread_create(&sniffer_thread, NULL,  connection_handler, (void*) new_sock) < 0) {
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
    int recv_size;

    // Deklarasi variabel yang digunakan untuk menyimpan
    // request dari pengguna.
    char client_message[16];

    // Ini adalah perintah yang nantinya akan dikirim
    // ke database MySQL, database akan merespon dari
    // apapun yang kita kirimkan kepadanya.
    // Query adalah cara program ini berkomunikasi dengan
    // database MySQL.
    char *query = malloc(64);

    // Apabila ada request dari pengguna
    while( (recv_size = recv(sock, client_message, 16, 0)) > 0 ) {
        // Koneksi dengan database MYSQL
        MYSQL *con = mysql_init(NULL);
        if (mysql_real_connect(con, "localhost", "admin", "adminpass", "alpro", 0, NULL, 0) == NULL) {
            finish_with_error(con);
        }

        // Mengisi variabel 'query' dengan query yang akan dikirimkan ke MySQL.
        // Maksud dari query ini ialah untuk mengambil (fetch) kolom 'Pesan' 
        // pada tabel 'msg', dimana ID dari pesan tersebut harus sama dengan
        // ID yang diminta oleh pengguna melalui request tadi.
        sprintf(query, "SELECT `Pesan` FROM msg WHERE `Id` = %d;", atoi(client_message));

        // Mengeksekusi query tersebut
        if (mysql_query(con, query))  {
            finish_with_error(con);
        }

        // Ini adalah variabel yang menyimpan respon dari MySQL
        MYSQL_RES *result = mysql_store_result(con);

        // Apabila MySQL tidak memberikan respon apa-apa, 
        // tampilkan error.
        if (result == NULL) {
            finish_with_error(con);
        }

        // Untuk menyimpan pesan yang tadi diambil dari database
        char *message = malloc(100);

        // Mengisi variabel ini dengan jumlah baris
        // yang dikirimkan oleh database MySQL.
        // Pada program ini, jumlah baris yang diterima 
        // pasti sama dengan 1 karena program ini hanya 
        // meminta satu pesan dengan ID tertentu saja.
        int num_fields = mysql_num_fields(result);
        MYSQL_ROW row;

        // Pesan yang diambil dari database akan
        // diletakkan di variabel 'message'
        int i;
        while ((row = mysql_fetch_row(result))) { 
            for(i = 0; i < num_fields; i++) {
                sprintf(message, "%s", row[i]);
            } 
            printf("\n"); 
        }

        // Membebaskan alokasi memori yang tadi untuk menyimpan
        // hasil pengambilan pesan dari database, dan menutup
        // koneksi dengan database
        mysql_free_result(result);
        mysql_close(con);

        // Mengirimkan kembali pesan yang diambil dari database
        // sebagai respon atas request yang dikirim pengguna.
        write(sock, message, strlen(message));

        // Program menghapus isi dari 'client_message'.
	    memset(client_message, 0, sizeof(client_message));
    }

    // Memberi kabar apabila koneksi kepada pengguna
    // sudah selesai
    if(recv_size == 0) {
        puts("Client disconnected");
        fflush(stdout);
    }

    else if(recv_size == -1) {
        perror("recv failed");
    }
         
    // Membebaskan alokasi memori untuk
    // socket programming
    free(socket_desc);
    
    // Menutup koneksi soket
	close(sock);
    return 0;
}