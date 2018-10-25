// broadcast 함수 선언
var broadcast = function(config) {
  var self = {
      userToken: uniqueToken(), // 유저 토큰 받아옴
      userName: getUrlParameter('user_name'),
      userProg: getUrlParameter('user_prog')
    },
    channels = '--',
    isbroadcaster,
    isGetNewRoom = true,
    sockets = [],
    defaultSocket = {},
    RTCDataChannels = [];
  //  defaultSocket = { }, // self 세팅

  // 기본 소켓 open 함수
  function openDefaultSocket() {
    defaultSocket = config.openSocket({
      onmessage: onDefaultSocketResponse,
      callback: function(socket) {
        defaultSocket = socket;
      }
    });
  }

  // defaultSocket에 들어가는 메소드인데 내가 입장한 방이 아닌 다른 방이 있다면 계속실행된다.
  function onDefaultSocketResponse(response) {
    if (response.userToken == self.userToken) return;
    new_client = response.userName;
    new_prog = response.userProg;
    // config의 onRoomFound를 호출해서 강의실 목록 갱신
    if (isGetNewRoom && response.roomToken && response.broadcaster) config.onRoomFound(response);

    // if (response.newParticipant && self.joinedARoom && self.broadcasterid == response.userToken) onNewParticipant(response.newParticipant);

    // 내방에 입장했을 때 나한테 알림 response는 상대방 self는 나
    if (response.userToken && response.joinUser == self.userToken && response.participant && channels.indexOf(response.userToken) == -1) {
      channels += response.userToken + '--';
      // 새로운 소켓 생성

      openSubSocket({
        isofferer: true,
        channel: response.channel || response.userToken,
        closeSocket: true
      });
    }
  }

  function openSubSocket(_config) {
    if (!_config.channel) return;
    var socketConfig = {
      channel: _config.channel,
      onmessage: socketResponse,
      onopen: function() {
        if (isofferer && !peer) initPeer();
        sockets[sockets.length] = socket;
      }
    };

    socketConfig.callback = function(_socket) {
      socket = _socket;
      this.onopen();
    };

    var socket = config.openSocket(socketConfig),
      isofferer = _config.isofferer,
      gotstream,
      video = document.createElement('video'),
      inner = {},
      peer;

    var peerConfig = {
      attachStream: config.attachStream,
      onICE: function(candidate) {
        socket.send({
          userName: self.userName,
          userToken: self.userToken,
          userProg: self.userProg,
          candidate: {
            sdpMLineIndex: candidate.sdpMLineIndex,
            candidate: JSON.stringify(candidate.candidate)
          }
        });
      },
      onChannelOpened: onChannelOpened,
      onChannelMessage: function(event) {
        config.onChannelMessage(JSON.parse(event.data));
      },
      onRemoteStream: function(stream) {
        if (!stream) return;

        video[moz ? 'mozSrcObject' : 'src'] = moz ? stream : webkitURL.createObjectURL(stream);
        video.play();

        _config.stream = stream;
        onRemoteStreamStartsFlowing();
      }
    };

    function initPeer(offerSDP) {
      if (!offerSDP) {
        peerConfig.onOfferSDP = sendsdp;
      } else {
        peerConfig.offerSDP = offerSDP;
        peerConfig.onAnswerSDP = sendsdp;
      }

      peer = RTCPeerConnection(peerConfig);
    }
    var ch = 0;

    function onChannelOpened(channel) {
      // if (getUrlParameter('state') != 'teacher')
      // RTCDataChannels[(RTCDataChannels.length + 1) / 2] = channel;
      // else
      if (getUrlParameter('state') == 'student')
        RTCDataChannels[0] = channel;
      else
        RTCDataChannels[RTCDataChannels.length] = channel;

      if (ch == 1 && getUrlParameter('state') == 'student') {
        channel.send(JSON.stringify({
          message: 'Hi, I\'m <strong>' + self.userName + '</strong>!',
          sender: self.userName
        }));
      }
      if (getUrlParameter('state') == 'teacher')
        channel.send(JSON.stringify({
          message: 'Hi, I\'m <strong>' + self.userName + '</strong>!',
          sender: self.userName
        }));
      ch = 1;

      // if (config.onChannelOpened) config.onChannelOpened(channel);

      if (isbroadcaster && channels.split('--').length > 3) {
        /* broadcasting newly connected participant for video-conferencing! */
        defaultSocket.send({
          newParticipant: socket.channel,
          userToken: self.userToken,
          userName: self.userName,
          userProg: self.userProg
        });
      }

      gotstream = true;
    }

    function afterRemoteStreamStartedFlowing() {
      gotstream = true;

      config.onRemoteStream({
        video: video,
        stream: _config.stream
      });

      /* closing subsocket here on the offerer side */
      if (_config.closeSocket) socket = null;
    }

    function onRemoteStreamStartsFlowing() {
      if (navigator.userAgent.match(/Android|iPhone|iPad|iPod|BlackBerry|IEMobile/i)) {
        // if mobile device
        return afterRemoteStreamStartedFlowing();
      }

      if (!(video.readyState <= HTMLMediaElement.HAVE_CURRENT_DATA || video.paused || video.currentTime <= 0)) {
        afterRemoteStreamStartedFlowing();
      } else setTimeout(onRemoteStreamStartsFlowing, 50);
    }

    function sendsdp(sdp) {
      sdp = JSON.stringify(sdp);
      var part = parseInt(sdp.length / 3);

      var firstPart = sdp.slice(0, part),
        secondPart = sdp.slice(part, sdp.length - 1),
        thirdPart = '';

      if (sdp.length > part + part) {
        secondPart = sdp.slice(part, part + part);
        thirdPart = sdp.slice(part + part, sdp.length);
      }

      socket.send({
        userName: self.userName,
        userToken: self.userToken,
        userProg: self.userProg,
        firstPart: firstPart
      });

      socket.send({
        userName: self.userName,
        userToken: self.userToken,
        userProg: self.userProg,
        secondPart: secondPart
      });

      socket.send({
        userName: self.userName,
        userToken: self.userToken,
        userProg: self.userProg,
        thirdPart: thirdPart
      });

      // socket.send({
      //   userToken: self.userToken,
      //   sdp: JSON.stringify(sdp)
      // });
    }

    function socketResponse(response) {
      if (response.userToken == self.userToken) return;
      if (response.firstPart || response.secondPart || response.thirdPart) {
        if (response.firstPart) {
          inner.firstPart = response.firstPart;
          if (inner.secondPart && inner.thirdPart) selfInvoker();
        }
        if (response.secondPart) {
          inner.secondPart = response.secondPart;
          if (inner.firstPart && inner.thirdPart) selfInvoker();
        }

        if (response.thirdPart) {
          inner.thirdPart = response.thirdPart;
          if (inner.firstPart && inner.secondPart) selfInvoker();
        }
      }

      if (response.candidate && !gotstream) {
        peer && peer.addICE({
          sdpMLineIndex: response.candidate.sdpMLineIndex,
          candidate: JSON.parse(response.candidate.candidate)
        });
      }

      // if (response.sdp) {
      //   inner.sdp = JSON.parse(response.sdp);
      //   selfInvoker();
      // }
      //
      if (response.left) {
        if (peer && peer.peer) {
          if (response.userState == 'teacher') {
            alert("강사님과의 접속이 끊겼습니다.\r\n 강의실 목록화면으로 이동합니다. \r\n잠시 후 다시 이용해 주십시오.");
            location.href = 'home.html?hash=' + getUrlParameter('hash') + '&state=' + getUrlParameter('state');
          } else {
            alert(response.userName + " 학생과의 연결이 종료되었습니다.");
          }

          peer.peer.close();
          peer.peer = null;
        }
      }
    }

    var invokedOnce = false;

    function selfInvoker() {
      if (invokedOnce) return;

      invokedOnce = true;

      inner.sdp = JSON.parse(inner.firstPart + inner.secondPart + inner.thirdPart);
      if (isofferer) peer.addAnswerSDP(inner.sdp);
      else initPeer(inner.sdp);
    }
  }

  function leave() {
    var length = sockets.length;
    for (var i = 0; i < length; i++) {
      var socket = sockets[i];
      if (socket) {
        socket.send({
          left: true,
          userToken: self.userToken,
          userName: self.userName,
          userState: getUrlParameter('state')
        });
        delete sockets[i];
      }
    }
  }

  window.onbeforeunload = function() {
    leave();
  };

  window.onkeyup = function(e) {
    if (e.keyCode == 116) leave();
  };

  function startBroadcasting() {
    defaultSocket && defaultSocket.send({
      roomToken: self.roomToken,
      roomName: self.roomName,
      broadcaster: self.userToken,
      userName: self.userName,
      userProg: self.userProg
    });
    setTimeout(startBroadcasting, 3000);
  }

  function uniqueToken() {
    var s4 = function() {
      return Math.floor(Math.random() * 0x10000).toString(16);
    };
    return s4() + s4() + "-" + s4() + "-" + s4() + "-" + s4() + "-" + s4() + s4() + s4();
  }

  openDefaultSocket();
  return {
    createRoom: function(_config) {

      self.roomName = _config.roomName || 'Anonymous';
      self.roomToken = uniqueToken();
      if (_config.userName) self.userName = _config.userName;
      if (_config.userProg) self.userProg = _config.userProg;

      isbroadcaster = true;
      isGetNewRoom = false;
      startBroadcasting();
    },
    joinRoom: function(_config) {
      self.roomToken = _config.roomToken;
      if (_config.userName) self.userName = _config.userName;
      if (_config.userProg) self.userProg = _config.userProg;
      isGetNewRoom = false;

      self.joinedARoom = true;
      self.broadcasterid = _config.joinUser;

      openSubSocket({
        channel: self.userToken
      });

      defaultSocket.send({
        participant: true,
        userName: self.userName,
        userToken: self.userToken,
        userProg: self.userProg,
        joinUser: _config.joinUser
      });
    },
    send: function(message) {
      // if (getUrlParameter('state') == 'teacher')
      // var length = (RTCDataChannels.length+1)/2;
      // else
      var length = RTCDataChannels.length;
      var data = JSON.stringify({
        message: message,
        sender: self.userName,
        userProg: self.userProg
      });
      if (!length) return;
      for (var i = 0; i < length; i++) {
        if (RTCDataChannels[i].readyState == 'open') {
          // alert(length);
          RTCDataChannels[i].send(data);
        }
      }
    }
  };
};

var getUrlParameter = function getUrlParameter(sParam) {
  var returnValue;
  var url = location.href;
  var parameters = (url.slice(url.indexOf('#') + 1, url.length)).split('&');
  for (var i = 0; i < parameters.length; i++) {
    var varName = parameters[i].split('=')[0];
    if (varName.toUpperCase() == sParam.toUpperCase()) {
      returnValue = parameters[i].split('=')[1];
      // alert(returnValue);
      return decodeURIComponent(returnValue);
    }
  }
};
