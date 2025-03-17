#include <openssl/ssl.h>
#include <openssl/err.h>
#include <iostream>
#include <winsock2.h>

void InitOpenSSL() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void CleanupOpenSSL() {
    EVP_cleanup();
}

int main() {
    // Initialize OpenSSL
    InitOpenSSL();

    // Load SSL context
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);

    if (!ctx) {
        std::cerr << "Failed to create SSL context" << std::endl;
        ERR_print_errors_fp(stderr);
        return 1;
    }

    std::cout << "OpenSSL Initialized Successfully!" << std::endl;

    // Cleanup OpenSSL
    SSL_CTX_free(ctx);
    CleanupOpenSSL();
    return 0;
}
