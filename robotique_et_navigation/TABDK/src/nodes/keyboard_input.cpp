#include "ros/ros.h"
#include "std_msgs/Int8.h"
#include "termios.h"
#include "../parametres.cpp"

using namespace ros;
using std_msgs::Int8;

Params params;

int getch();
Int8 keyboard_input;

int main(int argc, char **argv) {
  init(argc, argv, "keyboard_input");

  NodeHandle nh;
  params.load(nh);
  Publisher key_input_pub =
      nh.advertise<Int8>("/keyboard_input", 1000);

  Rate loop_rate(params.frequence);

  while (ok()) {
    const int key = getch();
    keyboard_input.data = key;
    key_input_pub.publish(keyboard_input);
    spinOnce();
    loop_rate.sleep();
  }
  return 0;
}

int getch() {
  static struct termios oldt, newt;
  tcgetattr(STDIN_FILENO, &oldt); // save old settings
  newt = oldt;
  newt.c_lflag &= ~(ICANON);               // disable buffering
  tcsetattr(STDIN_FILENO, TCSANOW, &newt); // apply new settings

  int c = getchar(); // read character (non-blocking)

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // restore old settings
  return c;
}
