//Oh no
//CLUNC
//Clunc Likes Unassembling Nearly Ceaselessly
//Some almalgamation of code from tutorials and example codes stitched together to kinda host a web server
//Duct-Taped by Lori_Madoka
//Oh yh you may need to compile it if ur  acc reading this so like: g++ -o testclunc testclunc.cpp -lstdc++

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <regex>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <future>
#include <queue>
#include <functional>
#include <condition_variable>
#include <map>
#include <string>

class ThreadPool {
    //I have no idea how threading works
public:
    ThreadPool(size_t numThreads);
    ~ThreadPool();
    void enqueue(std::function<void()> task);

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    void workerThread();
};

ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back(&ThreadPool::workerThread, this);
    }
}

ThreadPool::~ThreadPool() {
    stop.store(true);
    condition.notify_all();
    for (std::thread &worker : workers) {
        worker.join();
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        tasks.push(std::move(task));
    }
    condition.notify_one();
}

void ThreadPool::workerThread() {
    while (!stop.load()) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] { return stop.load() || !tasks.empty(); });
            if (stop.load() && tasks.empty()) {
                return;
            }
            task = std::move(tasks.front());
            tasks.pop();
        }
        task();
    }
}

const size_t MAX_FILE_SIZE = 26214400; //limits to 25MB

void handleRequest(int clientSocket) {
    const int bufferSize = 2048;
    char buffer[bufferSize] = {0};
    std::string requestData;

    // Chunking!!
    while (true) {
        ssize_t bytesRead = read(clientSocket, buffer, bufferSize - 1);
        if (bytesRead == -1) {
            perror("Error reading from client socket");
            close(clientSocket);
            return;
        }
        if (bytesRead == 0) {
            break;
        }

        // Append received data to requestData string
        requestData.append(buffer, bytesRead);

        // Check if the requestData string contains the complete request
        if (requestData.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }

    printf("Request:\n%s\n", requestData.c_str());

    // Extract the requested filename from the request (assuming GET request)
    std::string filename = "index.html"; // Default filename
    char *p = strtok(const_cast<char *>(requestData.c_str()), " ");
    if (p != nullptr && strcmp(p, "GET") == 0) {
        p = strtok(NULL, " ");
        filename = std::string(p + 1); // Skip the leading '/'
        //make the default file the index file for less typing
        if (filename == "") {
            filename = "index.html";
     }
    }

    else if (p != nullptr && strcmp(p, "POST") == 0) {
            p = strtok(NULL, " ");
            if (p != nullptr && strcmp(p, "/upload") == 0) {
                // Extract filename from request headers
                size_t contentTypeStart = requestData.find("Content-Type:");
                size_t contentTypeEnd = requestData.find("\r\n", contentTypeStart);
                if (contentTypeStart != std::string::npos && contentTypeEnd != std::string::npos) {
                    std::string contentType = requestData.substr(contentTypeStart, contentTypeEnd - contentTypeStart);
                    std::cout << "Content-Type: " << contentType << std::endl; // Debug statement
                    size_t boundaryStart = contentType.find("boundary=") + 9; // Skip "boundary="
                    std::string boundary = contentType.substr(boundaryStart);
    
                    // Split request into parts using boundary
                    std::vector<std::string> parts;
                    size_t currentPos = requestData.find("\r\n\r\n") + 4; // Skip headers
                    while (true) {
                        size_t partEnd = requestData.find("\r\n--" + boundary, currentPos);
                        if (partEnd != std::string::npos) {
                            parts.push_back(requestData.substr(currentPos, partEnd - currentPos));
                            currentPos = partEnd + boundary.size() + 4; // Skip the boundary and CRLF
    
                            // Check if this is the last part (with "--" after the boundary)
                            if (requestData.substr(partEnd - 2, 2) == "--") {
                                break;
                            }
                        } else {
                            break;
                        }
                    }
    
                    // Process each part to extract filename and content
                    for (const auto& part : parts) {
                        size_t filenameStart = part.find("filename=\"") + 10;
                        size_t filenameEnd = part.find("\"", filenameStart);
                        if (filenameStart != std::string::npos && filenameEnd != std::string::npos) {
                            std::string filename = part.substr(filenameStart, filenameEnd - filenameStart);
    
                            // Skip content-type line and CRLF
                            size_t contentStart = part.find("\r\n\r\n") + 4;
    
                            // Save file
                            std::ofstream outputFile(filename, std::ios::binary);
                            if (outputFile) {
                                outputFile.write(part.c_str() + contentStart, part.size() - contentStart);
                                outputFile.close();
                                std::cout << "File successfully saved as: " << filename << std::endl;
                            } else {
                                std::cerr << "Error: Unable to save file " << filename << std::endl;
                            }
                        } else {
                            std::cerr << "Error: Unable to extract filename from part" << std::endl;
                        }
                    }
                } else {
                    std::cerr << "Error: Unable to extract Content-Type header" << std::endl;
                }
    
                // Send response to client
                std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
                send(clientSocket, response.c_str(), response.size(), 0);
                close(clientSocket);
                return;
            }
        }

    // Open the requested file
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        std::string httpResponse = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(clientSocket, httpResponse.c_str(), httpResponse.size(), 0);
        close(clientSocket);
        return;
    }

    // Read file
    std::stringstream contentStream;
    contentStream << file.rdbuf();
    std::string content = contentStream.str();
    //std::string mimeType = getMimeType(filename);

    // Prep the HTTP response
    std::string httpResponse = "HTTP/1.1 200 OK\r\n";
    httpResponse += "Content-Length: " + std::to_string(content.size()) + "\r\n";
    //grab the  string of the file exptension for comparison later
    int dotpos = 0;
    char c;
    std::string ext;
    while (dotpos<filename.length()){
        std::cout << "checking character of filename: ";
        if (filename[dotpos] != '.'){
            ext+=filename[dotpos];
        }
        else{
            ext = "";
        }
        dotpos+=1;
        std::cout << ext;
        std::cout << "" << std::endl;
    }
    std::cout << "" << std::endl;

    std::cout << ":" << ext << ":" << std::endl;
    std::cout << "length of ext is " << ext.length() << std::endl;

    //printf("the magic character is %d\n", ext.c_str()[3]);
    //result of having bad loop in  the dotfinder.



    std::string ico = "ico";
    //ext now has definition yippeeee
    //but it is  being frustration 
    //will just turn it into a char
    
    if (ext == "ico"){
        std::cout << "entered ico selection" << std::endl;
        httpResponse += "Content-Type: image/x-icon\r\n\r\n";
    }
    else if (ext == "mkv"){
        std::cout << "eneteresthe video yippeeee" << std::endl;
        httpResponse += "Content-Type: video/x-matroska\r\n\r\n";
    }
    else if (ext == "jpeg" || "jpg"){
        std::cout << "entered jpeg selection" << std::endl;
        httpResponse += "Content-Type: image/jpeg\r\n\r\n";
    }
    else if (ext == "apng"){
        std::cout << "entered apng selection" << std::endl;
        httpResponse += "Content-Type: image/apng\r\n\r\n";
    }
    else if (ext == ""){
        std::cout << "entered png selection" << std::endl;
        httpResponse += "Content-Type: image/png\r\n\r\n";
    }
    else if (ext == "pdf"){
        std::cout << "entered pdf selection" << std::endl;
        httpResponse += "Content-Type: application/pdf\r\n\r\n";
    }
    else{
        std::cout << "Entered standard selection" << std::endl;
        httpResponse += "Content-Type: text/html\r\n\r\n";
        //"Content-Type: text/html\r\n\r\n"
    }
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
    chroot(".");
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
        close(serverSocket);
        return 1;
    }
    //Dont forget to check your firewall isnt blocking your port from porting (sudo ufw allow 8080)

    // Listen for incoming connections
    if (listen(serverSocket, 10) == -1) {
        perror("Error listening");
        close (serverSocket);
        return 1;
    }

    std::cout << "Grabbing info from port 8080...\n";

    ThreadPool threadPool(4);  // Create a thread pool with 4 worker threads

    // Accept incoming connections and handle requests
    while (true) {
        int clientSocket = accept(serverSocket, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (clientSocket == -1) {
            perror("Error accepting connection");
            continue;
        }

        // Enqueue the handling of the client request to the thread pool
        threadPool.enqueue([clientSocket]() { handleRequest(clientSocket); });
    }

    return 0;
}

