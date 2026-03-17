#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

#include "../../include/telemetry.h"
#include "../../include/telemetry_parser.h"
#include "../../include/state.h"
#include "../../include/dashboard_manager.h"
#include "emulator_tft.h"

int main() {
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) { perror("socket"); return 1; }

  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(20777);

  if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
    perror("bind"); return 1;
  }

  std::cout << "Emulator listening UDP on 0.0.0.0:20777" << std::endl;

  uint8_t buf[1500];
  EmulatorTFT:;
  TFT_eSPI tft;
  dashboard_init(&tft);
  StateManager state;

  while (true) {
    ssize_t n = recv(sockfd, buf, sizeof(buf), 0);
    if (n <= 0) continue;
    TelemetryFrame frame = state.current();
    telemetry_parse(buf, (size_t)n, frame);
    state.updateFrame(frame);
    // Print parsed values
    const auto &f = state.current();
    std::cout << "--- Frame id="<< f.frameIdentifier <<" speed="<< f.telemetry.speedKmh <<" rpm="<< f.telemetry.rpm
              <<" gear="<< (int)f.telemetry.gear <<" throttle="<<(int)f.telemetry.throttle <<" brake="<<(int)f.telemetry.brake
              <<" mfd="<<(int)f.telemetry.mfdPanelIndex <<" pit="<<(int)f.lap.pitStatus <<" ers="<<f.status.ersEnergy <<"\n";
    dashboard_update(state);
  }

  close(sockfd);
  return 0;
}
