// ===================== Датчики ===============================

var oneFunction = function () {
	
	setInterval(function () {
		var xhttp = new XMLHttpRequest();
		xhttp.onreadystatechange = function () {
			if (this.readyState == 4 && this.status == 200) {
				document.getElementById("in_1").innerHTML = this.responseText; // Отправляем температуру в html по id "in_1"
			}
		};
		xhttp.open("GET", "/IN1", true); // Делаем запрос о температуре в скетч.
		xhttp.send();
	}, 5000);
	
	setInterval(function () {
		var xhttp = new XMLHttpRequest();
		xhttp.onreadystatechange = function () {
			if (this.readyState == 4 && this.status == 200) {
				document.getElementById("in_2").innerHTML = this.responseText;
			}
		};
		xhttp.open("GET", "/IN2", true);
		xhttp.send();
	}, 5000);
	
	setInterval(function () {
		var xhttp = new XMLHttpRequest();
		xhttp.onreadystatechange = function () {
			if (this.readyState == 4 && this.status == 200) {
				document.getElementById("in_3").innerHTML = this.responseText;
			}
		};
		xhttp.open("GET", "/IN3", true);
		xhttp.send();
	}, 5000);
	
	setInterval(function () {
		var xhttp = new XMLHttpRequest();
		xhttp.onreadystatechange = function () {
			if (this.readyState == 4 && this.status == 200) {
				document.getElementById("in_4").innerHTML = this.responseText;
			}
		};
		xhttp.open("GET", "/IN4", true);
		xhttp.send();
	}, 5000);
	
	setInterval(function () {
		var xhttp = new XMLHttpRequest();
		xhttp.onreadystatechange = function () {
			if (this.readyState == 4 && this.status == 200) {
				document.getElementById("in_5").innerHTML = this.responseText;
			}
		};
		xhttp.open("GET", "/IN5", true);
		xhttp.send();
	}, 5000);
	
	setInterval(function () {
		var xhttp = new XMLHttpRequest();
		xhttp.onreadystatechange = function () {
			if (this.readyState == 4 && this.status == 200) {
				document.getElementById("in_6").innerHTML = this.responseText;
			}
		};
		xhttp.open("GET", "/IN6", true);
		xhttp.send();
	}, 5000);
	
	setInterval(function () {
		var xhttp = new XMLHttpRequest();
		xhttp.onreadystatechange = function () {
			if (this.readyState == 4 && this.status == 200) {
				document.getElementById("in_7").innerHTML = this.responseText;
			}
		};
		xhttp.open("GET", "/IN7", true);
		xhttp.send();
	}, 5000);
	
	setInterval(function () {
		var xhttp = new XMLHttpRequest();
		xhttp.onreadystatechange = function () {
			if (this.readyState == 4 && this.status == 200) {
				document.getElementById("in_8").innerHTML = this.responseText;
			}
		};
		xhttp.open("GET", "/IN8", true);
		xhttp.send();
	}, 5000);
	
// 	// Выводим остаток времени до срабатывания таймера №3
// 	setInterval(function () {
// 		var xhttp = new XMLHttpRequest();
// 		xhttp.onreadystatechange = function () {
// 			if (this.readyState == 4 && this.status == 200) {
// 				document.getElementById("dateTime3").innerHTML = this.responseText; // Отправляем время в html
// 			}
// 		};
// 		xhttp.open("GET", "/timeLeft3", true);   // Получаем данные о остатке времени до срабатывания таймера из скетча.
// 		xhttp.send();
// 	}, 1000);
	
 }

oneFunction()

// ======================== Кнопки =====================================

var gateway = `ws://${window.location.hostname}/ws`; // Шлюз-это точка входа в интерфейс WebSocket, возвращает текущий адрес страницы (IP-адрес веб-сервера).
var websocket;                                       // Новая глобальная переменная под названием websocket
window.addEventListener('load', onLoad);             // Добавляем прослушиватель событий, который будет вызывать функцию onload при загрузке веб-страницы.

/* инициализирует соединение WebSocket на шлюзе, определенном ранее. Мы также назначаем 
   несколько функций обратного вызова при открытии, закрытии соединения WebSocket или 
   получении сообщения.*/

function initWebSocket() {
	console.log('Попытка открыть соединение с веб-сокетом...');
	websocket = new WebSocket(gateway);
	websocket.onopen = onOpen;
	websocket.onclose = onClose;
	websocket.onmessage = onMessage;
}

// Событие OnOpen определяет, что новый клиент запросил доступ и выполняет первоначальное приветсвие

function onOpen(event) {
	console.log('Соединение открыто');
}

/* Событие OnClose возникает всякий раз, когда клиент отключен. Клиент удаляется из списка и информирует 
   остальных клиентов об отключении. через 2000 миллисекунд (2 секунды).*/

function onClose(event) {
	console.log('Соединение закрыто');
	setTimeout(initWebSocket, 2000);
}

/* Оператор switch сравнивает значение переменной event.data, со значением, определенном в операторах case.
	Когда найден оператор case, значение которого равно значению переменной, выполняется программный код в 
	этом операторе. document.getElementById("***") это поиск в HTML по id="***", после изменяем содержимое элемента 
    с помощью innerHTML на текущие показания (маркировка кнопок ON или OFF, картинок, даты и времени и др. */

function onMessage(event) {

//  document.getElementById("dateTime").innerHTML = event.data; // Отправляем температуру в html
//	var arrayS = event.data.split("&");
//	document.getElementById("dateTime1").innerHTML = arrayS[0];
//	document.getElementById("dateTime2").innerHTML = arrayS[1];
//	document.getElementById("dateTime3").innerHTML = arrayS[2];
    // let data = event.data.split(',');
    // document.getElementById("dateTime1").innerHTML = data[0];
    // document.getElementById("dateTime2").innerHTML = data[1];
    // document.getElementById("dateTime3").innerHTML = data[2];
    // setTimeout(function () { doSend('getData'); }, period);
	
	switch (event.data) {
		case '0': document.getElementById("state1").innerHTML = "OFF"; document.getElementById('button1').style.backgroundColor = "#c90411"; document.getElementById("winimg").src = "https://kzpm.org/img/picture_main/down.gif"; break
		case '1': document.getElementById("state1").innerHTML = "ON &nbsp;"; document.getElementById('button1').style.backgroundColor = "#04b50a"; document.getElementById("winimg").src = "https://kzpm.org/img/picture_main/up.gif"; break
		case '2': document.getElementById("state2").innerHTML = "OFF"; document.getElementById('button2').style.backgroundColor = "#c90411"; break
		case '3': document.getElementById("state2").innerHTML = "ON &nbsp;"; document.getElementById('button2').style.backgroundColor = "#04b50a"; break
		case '4': document.getElementById("state3").innerHTML = "OFF"; document.getElementById('button3').style.backgroundColor = "#c90411"; break
		case '5': document.getElementById("state3").innerHTML = "ON &nbsp;"; document.getElementById('button3').style.backgroundColor = "#04b50a"; break
		case '6': document.getElementById("state4").innerHTML = "OFF"; document.getElementById('button4').style.backgroundColor = "#c90411"; break
		case '7': document.getElementById("state4").innerHTML = "ON &nbsp;"; document.getElementById('button4').style.backgroundColor = "#04b50a"; break
		case '8': document.getElementById("state5").innerHTML = "OFF"; document.getElementById('button5').style.backgroundColor = "#c90411"; break
		case '9': document.getElementById("state5").innerHTML = "ON &nbsp;"; document.getElementById('button5').style.backgroundColor = "#04b50a"; break
		case '10': document.getElementById("wether").src = "https://kzpm.org/img/picture_main/pic1.gif"; break
		case '11': document.getElementById("wether").src = "https://kzpm.org/img/picture_main/pic2.gif"; break
	}
}

function onLoad(event) { 
	initWebSocket(); 
	initButton(); 
}

/* Слушаем кнопку (html-->js) по ее идентификатору (button1) и добавляем команду событий 
   "click" (js-->js). Это означает, что при нажатии на кнопку вызывается функция переключения,
   а именно function toggle1(), function toggle2() и.т.д*/

function initButton() {
	document.getElementById('button1').addEventListener('click', toggle1);
	document.getElementById('button2').addEventListener('click', toggle2);
	document.getElementById('button3').addEventListener('click', toggle3);
	document.getElementById('button4').addEventListener('click', toggle4);
	document.getElementById('button5').addEventListener('click', toggle5);
}

// (js-->код) Отправляем команду на переключение с текстом "toggle1, toggle2 и.т.д"

function toggle1() {websocket.send('toggle1');} 
function toggle2() {websocket.send('toggle2');}
function toggle3() {websocket.send('toggle3');}
function toggle4() {websocket.send('toggle4');}
function toggle5() {websocket.send('toggle5');}

// Отправляем текущее время на вэб страницу. 
// var todays_date = new Date();    // Создаем переменную  todays_date
// document.getElementById("dateTime").innerHTML = todays_date;