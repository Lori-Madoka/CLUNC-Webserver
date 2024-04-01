//Oh no
//CLUNC
//Clunc Likes Unassembling Nearly Ceaselessly
//Some almalgamation of code from tutorials and example codes stitched together to kinda host a web server
//Duct-Taped by Lori_Madoka
//Oh yh you may need to compile it if ur  acc reading this so like: g++ -o CLUNC CLUNC.cpp -lstdc++

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

const size_t MAX_FILE_SIZE = 26214400; //limits to 25MB

void handleRequest(int clientSocket) {
    // look at request from the client
    char buffer[1024] = {0};
    ssize_t bytesRead = read(clientSocket, buffer, 1024);
    if (bytesRead == -1) {
        perror("Error reading from client socket");
        close(clientSocket);
        return;
    }
    printf("Request:\n%s\n", buffer);

    // Extract the requested filename from the request (assuming GET request)
    std::string filename = "index.html"; // Default filename
    char *p = strtok(buffer, " ");
    if (p != nullptr && strcmp(p, "GET") == 0) {
        p = strtok(NULL, " ");
        filename = std::string(p + 1); // Skip the leading '/'
	//make the default file the index file for less typing
	if (filename == "") {
		filename = "index.html";
	}
    }
    else if (p != nullptr && strcmp(p, "POST") == 0) {
	//If request is a post request its time to NOT use a library for yippee
	//Extract filename
	size_t filenameStart = std::string(buffer).find("filename=\"") + 10;
	size_t filenameEnd = std::string(buffer).find("\"", filenameStart);
	filename = std::string(buffer).substr(filenameStart, filenameEnd - filenameStart);
	
	//Grab content of file to upload
	std::string content(buffer + bytesRead);
	size_t contentStart = content.find("\r\n\r\n") + 4;
	content = content.substr(contentStart);

	//Make sure file isnt extra Thicc
	if (content.size() > MAX_FILE_SIZE) {
		std::cerr << "Error: File size a bit THICC" << std::endl;
		close(clientSocket);
		return;
	}
	
	//Save file
	std::ofstream outputFile(filename, std::ios::binary);
	if (outputFile) {
		outputFile.write(content.c_str(), content.size());
		outputFile.close();
		std::cout << "File successfully saved as: " << filename << std::endl;
	}
	else {
		std::cerr << "Error: Did not save file " << filename << std::endl;
	}

	//Acc send the client a response
	std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
	send(clientSocket, response.c_str(), response.size(), 0);
	close(clientSocket);
	return;
    }

    // Open the requested file
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return;
    }

    // Read file
    std::stringstream contentStream;
    contentStream << file.rdbuf();
    std::string content = contentStream.str();

    // Prep the HTTP response
    std::string httpResponse = "HTTP/1.1 200 OK\r\n";
    httpResponse += "Content-Length: " + std::to_string(content.size()) + "\r\n";
    httpResponse += "Content-Type: text/html\r\n\r\n";
    httpResponse += content;

    // Send HTTP response to the client
    ssize_t bytesSent = send(clientSocket, httpResponse.c_str(), httpResponse.size(), 0);
    if (bytesSent == -1) {
        perror("Error sending response to client socket");
    }

    // Close the client socket
    close(clientSocket);
}

int main() {
    chroot("/home/Madoka/Downloads/TestingCpp");
    // Create socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        perror("Error creating socket");
        return 1;
    }

    // Bind socket to localhost:8080
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; //now all network interfaces get addresses assigned to them
    address.sin_port = htons(8080);
    if (bind(serverSocket, (struct sockaddr *)&address, addrlen) == -1) {
        perror("Error binding socket");
        return 1;
    }
    //Dont forget to check your firewall isnt blocking your port from porting (sudo ufw allow 8080)

    // Listen for incoming connections
    if (listen(serverSocket, 10) == -1) {
        perror("Error listening");
        return 1;
    }

    std::cout << "Grabbing info from port 8080...\n";

    // Accept incoming connections and handle requests
    while (true) {
        int clientSocket = accept(serverSocket, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (clientSocket == -1) {
            perror("Error accepting connection");
            continue;
        }

        handleRequest(clientSocket);
    }

    return 0;
}

