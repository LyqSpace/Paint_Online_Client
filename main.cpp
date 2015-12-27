#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

using namespace std;

#include <opencv2/opencv.hpp>

using namespace cv;

const char *server_IP = "127.0.0.1";
const short server_port = 58520;
const Size boardSize = Size(600, 400);
const int platte_height = 10;

int server_socket;

Scalar brushColor;
int mouseState;
Point lastP;
Mat myBoard;
char windowName[100] = "Paint Board";

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

void setup_socket() {

	sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(server_IP);
	server_addr.sin_port = htons(server_port);

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		error("ERR : Create server socket failed.");
	}

	if (connect(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		error("ERR : Connect to server failed.");
	}

	cout << "INFO : Connect to server successful !" << endl;
}

void* recv_thread(void*) {

	while (true) {
		char msg[100] = {0};
		if (recv(server_socket, msg, sizeof(msg), 0) <= 0) return NULL;

		if (!isdigit(msg[0])) {
			cout << msg << endl;
			continue;
		}
		Point p0, p1;
		Scalar c;
		int thick = -1;
		char *params = strtok(msg, " ");
		p0.x = atoi(params);
		params = strtok(NULL, " ");
		p0.y = atoi(params);
		params = strtok(NULL, " ");
		p1.x = atoi(params);
		params = strtok(NULL, " ");
		p1.y = atoi(params);
		params = strtok(NULL, " ");
		for (int k = 0; k < 3; k++) {
			c.val[k] = atoi(params);
			params = strtok(NULL, " ");
		}
		thick = atoi(params);

		if (thick <= 0 || thick > 10 || c.val[0] < 0 || c.val[0] > 255 ) continue;
		line(myBoard, p0, p1, c, thick);
		imshow(windowName, myBoard);
	}
}

void setup_receiver() {
	pthread_t pid;
	pthread_create(&pid, 0, recv_thread, 0);
	cout << "INFO : Set up receiver successful !" << endl;
}

double Hue2RGB(double v1, double v2, double vH) {

	if (vH < 0) vH += 1;
	if (vH > 1) vH -= 1;
	if (6.0 * vH < 1) return v1 + (v2 - v1) * 6.0 * vH;
	if (2.0 * vH < 1) return v2;
	if (3.0 * vH < 2) return v1 + (v2 - v1) * ((2.0 / 3.0) - vH) * 6.0;
	return v1;
}

Scalar HSL2BGR(double H) {

	double v1, v2, R, G, B, S = 1, L = 0.5;

	v2 = (L + S) - (L * S);
	v1 = 2 * L - v2;

	R = 255 * Hue2RGB(v1, v2, H + (1.0/3.0));
	G = 255 * Hue2RGB(v1, v2, H);
	B = 255 * Hue2RGB(v1, v2, H - (1.0/3.0));

	return Scalar(B, G, R);
}

void initBoard() {

	myBoard = Mat(boardSize, CV_8UC3, Scalar(255, 255, 255));
	int width_bias = 108;
	double width_step = (boardSize.width - 8 - width_bias) / 255.0;
	Vec3b color;
	Scalar c;
	for (int i = 0; i < 255; i++) {

		c = HSL2BGR(i / 255.0);
		for (int k = 0; k < 3; k++) color.val[k] = c.val[k];

		for (int x = width_step * i; x < width_step * (i+1); x++) {
			for (int y = 0; y < platte_height; y++) {
				myBoard.ptr<Vec3b>(y)[width_bias + x] = color;
			}
		}
	}

}

void onTrackbar(int colorPos, void*) {

	brushColor = HSL2BGR(colorPos / 255.0);
}

void sendMsg(const Point &p0, const Point &p1, const Scalar &c, const int &thick) {

	char msg[100];
	sprintf(msg, "%d %d %d %d %d %d %d %d", p0.x, p0.y, p1.x, p1.y,
											(int)c.val[0], (int)c.val[1], (int)c.val[2],
											thick);
	send(server_socket, msg, sizeof(msg), 0);
}

void drawBoard(const Point &p0, const Point &p1,
			   const Scalar color = Scalar(255, 255, 255), const int thick = 20) {

	line(myBoard, p0, p1, color, thick);
	sendMsg(p0, p1, color, thick);
	imshow(windowName, myBoard);
}

void onMouse(int event, int x, int y, int, void*) {

	Point curP = Point(x, y);

	switch (event) {
	case EVENT_LBUTTONDOWN:
		mouseState = 1;
		lastP = Point(x, y);
		drawBoard(lastP, curP, brushColor, 2);
		break;
	case EVENT_LBUTTONUP:
		mouseState = 0;
		break;
	case EVENT_RBUTTONDOWN:
		mouseState = 2;
		lastP = Point(x, y);
		drawBoard(lastP, curP);
		break;
	case EVENT_RBUTTONUP:
		mouseState = 0;
		break;
	case EVENT_MOUSEMOVE:
		if (x < 0 || x >= boardSize.width || y <= platte_height || y >= boardSize.height) {
			mouseState = 0;
		}
		if (mouseState == 0) break;

		if (mouseState == 1) {
			drawBoard(lastP, curP, brushColor, 2);
		} else {
			drawBoard(lastP, curP);
		}
		lastP = curP;
		break;
	default:
		break;
	}
}

void setup_board(const char *userName) {

	strcat(windowName, " - ");
	strcat(windowName, userName);
	namedWindow(windowName);

	const char *trackbarName = "Brush Color : ";
	int colorPos = 0;
	int trackbarMax = 255;
	brushColor = Scalar(0, 0, 255);
	createTrackbar(trackbarName, windowName, &colorPos, trackbarMax, onTrackbar);

	mouseState = 0;
	setMouseCallback(windowName, onMouse);

	initBoard();
	imshow(windowName, myBoard);

	while (waitKey(1) != 27);

}

int main(int argc, char **argv) {

	if (argc < 2) {
		error("ERR : Please input an username.");
	}

	setup_socket();
	setup_receiver();
	send(server_socket, argv[1], sizeof(argv[1]), 0);
	setup_board(argv[1]);



	return 0;
}

