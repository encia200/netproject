var myVideo;
// config에 openSocket,onRemoteStream,onRoomFound 함수 넣음
var config = {
  openSocket: function(config) {
    var SIGNALING_SERVER = 'https://webrtcweb.com:9559/';

    // config.channel = config.channel || location.href.replace(/\/|:|#|%|\.|\[|\]/g, '');
    config.channel = config.channel || getUrlParameter('room_name');
    // alert(config.channel);
    var sender = Math.round(Math.random() * 999999999) + 999999999;

    io.connect(SIGNALING_SERVER).emit('new-channel', {
      channel: config.channel,
      sender: sender
    });

    var socket = io.connect(SIGNALING_SERVER + config.channel);
    socket.channel = config.channel;
    socket.on('connect', function() {
      if (config.callback) config.callback(socket);
    });

    socket.send = function(message) {
      socket.emit('message', {
        sender: sender,
        data: message
      });
    };

    socket.on('message', config.onmessage);
  },
  onRemoteStream: function(media) {
    var video = media.video;
    video.setAttribute('controls', false);

    myScreen.insertBefore(video, myScreen.firstChild);
    addBtn(video);
  },
  onRoomFound: function(room) {
    if (check == 1) return;
    var alreadyExist = document.getElementById(room.broadcaster);
    if (alreadyExist) return;

    if (getUrlParameter('room_name') != room.roomName) return;

    captureUserMedia(function() {
      broadcastUI.joinRoom({
        roomToken: room.roomToken,
        joinUser: room.broadcaster,
        userName: getUrlParameter('user_name'),
        userProg: getUrlParameter('user_prog')
      });
    });
    hideUnnecessaryStuff();
    check = 1;
  },
  onChannelOpened: function( /* channel */ ) {
    hideUnnecessaryStuff();
  },
  onChannelMessage: function(data) {
    if (!chatOutput) return;

    var tr = document.createElement('tr');
    tr.innerHTML =
      '<td style="width:40%;">' + data.sender + '</td>' +
      '<td>' + data.message + '</td>';

    // chatOutput.insertBefore(tr, chatOutput.firstChild);
    chatOutput.appendChild(tr);
    chatOutput.scrollTop = chatOutput.scrollHeight;

    if(data.message == 'Help me') {
      quest.style.display = "block";
    }
  }
};

function createButtonClickHandler() {

  captureUserMedia(function() {
    broadcastUI.createRoom({
      userName: getUrlParameter('user_name'),
      roomName: getUrlParameter('room_name'),
      userProg: getUrlParameter('user_prog')
    });
  });
  hideUnnecessaryStuff();
}

function captureUserMedia(callback) {
  var video = document.createElement('video');
  video.setAttribute('controls', false);
  video.setAttribute('autoplay', true);
  // video.setAttribute('controls', true);

  var mediaElement = getHTMLMediaElement(video, {
    // title: 'Click "Get Mixed Stream" button',
    buttons: [],
    showOnMouseEnter: false,
    width: document.getElementById('participants').parentNode.clientWidth
  });
  participants.appendChild(mediaElement);

  var div = document.createElement('section');

  mediaElement.media.parentNode.appendChild(div);

  div.appendChild(mediaElement.media);

  var videoPreview = mediaElement.media;
  myVideo = videoPreview;
  videoPreview.width = 300;

  getScreenId(function(error, sourceId, screen_constraints) {
    navigator.mediaDevices.getUserMedia(screen_constraints).then(function(screenStream) {
      navigator.mediaDevices.getUserMedia({
        video: true,
        audio: true
      }).then(function(cameraStream) {
        screenStream.fullcanvas = true;
        // screenStream.width = screen.width; // or 3840
        // screenStream.height = screen.height; // or 2160

        screenStream.width = 1000;
        screenStream.height = 562;

        cameraStream.width = parseInt((30 / 100) * screenStream.width);
        cameraStream.height = parseInt((30 / 100) * screenStream.height);
        cameraStream.top = screenStream.height - cameraStream.height;
        cameraStream.left = screenStream.width - cameraStream.width;

        var mixer = new MultiStreamsMixer([screenStream, cameraStream]);

        // peer.addStream(audioMixer.getMixedStream());

        mixer.frameInterval = 1;
        mixer.startDrawingFrames();

        // 스트림 획득 mixer.getMixedStream()
        setSrcObject(mixer.getMixedStream(), videoPreview);

        // config.attachStream : 원격에 보낼 스트림 설정
        config.attachStream = mixer.getMixedStream();
        callback && callback();

        addStreamStopListener(screenStream, function() {
          mixer.releaseStreams();
          videoPreview.pause();
          videoPreview.src = null;

          cameraStream.getTracks().forEach(function(track) {
            track.stop();
          });
        });
      });
    });
  });
}

function addStreamStopListener(stream, callback) {
  var streamEndedEvent = 'ended';
  if ('oninactive' in stream) {
    streamEndedEvent = 'inactive';
  }
  stream.addEventListener(streamEndedEvent, function() {
    callback();
    callback = function() {};
  }, false);
  stream.getAudioTracks().forEach(function(track) {
    track.addEventListener(streamEndedEvent, function() {
      callback();
      callback = function() {};
    }, false);
  });
  stream.getVideoTracks().forEach(function(track) {
    track.addEventListener(streamEndedEvent, function() {
      callback();
      callback = function() {};
    }, false);
  });
}
//
var check;
var new_client;
var new_prog;
/* on page load: get public rooms */
var broadcastUI = broadcast(config);

/* UI specific */
var participants = document.getElementById("participants") || document.body;
var myScreen = document.getElementById("myScreen");
var startConferencing = document.getElementById('gogo');
var roomsList = document.getElementById('rooms-list');

if (startConferencing) startConferencing.onclick = createButtonClickHandler;

var chatOutput = document.getElementById('chat-output');

function hideUnnecessaryStuff() {
  var visibleElements = document.getElementsByClassName('visible'),
    length = visibleElements.length;
  for (var i = 0; i < length; i++) {
    visibleElements[i].style.display = 'none';
  }

  var chatTable = document.getElementById('chat-table');
  if (chatTable) chatTable.style.display = 'block';
  if (chatOutput) chatOutput.style.display = 'block';
  if (chatMessage) chatMessage.disabled = false;
}

var chatMessage = document.getElementById('chat-message');
if (chatMessage)
  chatMessage.onchange = function() {
    broadcastUI.send(this.value);
    var tr = document.createElement('tr');
    tr.innerHTML =
      '<td style="width:40%;">You:</td>' +
      '<td>' + chatMessage.value + '</td>';

    // chatOutput.insertBefore(tr, chatOutput.firstChild);
    chatOutput.appendChild(tr);
    chatOutput.scrollTop = chatOutput.scrollHeight;
    chatMessage.value = '';
  };

var prev;

function question_click() {
  broadcastUI.send('Help me');
  var tr = document.createElement('tr');
  tr.innerHTML =
  '<td style="width:40%;">You:</td>' +
  '<td>Help me</td>';

  chatOutput.appendChild(tr);
  chatOutput.scrollTop = chatOutput.scrollHeight;

}

var quest;
function addBtn(video) {
  var tr = document.createElement("tr");
  var td1 = document.createElement("td");
  var td2 = document.createElement("td");
  var td3 = document.createElement("td");
  var td4 = document.createElement("td");
  var id = document.createElement("span");
  var prog = document.createElement("span");
  var btn = document.createElement("input");
  id.innerHTML = new_client;
  prog.innerHTML = new_prog;
  // alert(new_client);
  // alert(new_prog);
  quest = document.createElement("img");
  quest.setAttribute('src', 'img/question.png');
  quest.style.display = "none";
video.style.width = '-webkit-fill-available';
video.style.height = '-webkit-fill-available';
  btn.setAttribute('type', 'radio');
  btn.setAttribute('value', 'false');
  btn.setAttribute('name', 'userList');
  btn.addEventListener("click", function() {
    if (prev != btn) {
      $(prev).trigger("click");
      $("input:radio[name='userList']:radio[value='true']").prop('checked', false);
      prev.setAttribute('value', 'false');
    }
    btn.setAttribute('value', 'true');
    $("input:radio[name='userList']:radio[value='true']").prop('checked', true);
    if (video.style.display == "none") {
      video.play();
      video.style.display = "block";
    } else {
      video.style.display = "none";
      video.pause();
    }
    if (quest.style.display = "block") {
      quest.style.display = "none";
    }
    prev = btn;

  }, false);
  td1.appendChild(id);
  td2.appendChild(prog);
  td3.appendChild(btn);
  td4.appendChild(quest);
  tr.appendChild(td1);
  tr.appendChild(td2);
  tr.appendChild(td3);
  tr.appendChild(td4);
  document.getElementById('list-bd').appendChild(tr);
  if ($('#list-tb >tbody').children().length == 1) {
    prev = btn;
    btn.setAttribute('checked', 'true');
  } else {
    video.style.display = "none";
    video.pause();
  }
}
window.onload = function() {
  var s = getUrlParameter('state');
  if(s=='teacher')
    createButtonClickHandler();
}
function toggle_visibility(id) {
  var e = document.getElementById(id);
  if (e.style.visibility == 'hidden') {
    e.style.visibility = 'visible';
    myVideo.play();
  } else {
    e.style.visibility = 'hidden';
    myVideo.pause();
  }
}
