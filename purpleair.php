<?php

// PHP Script for reading Purpleair data for web pages
// Copyright (c) 2018 Chris Formeister. www.copperwoodwx.com/www.cfcsaz.com
// Free to Distribute as long as these credits remain
//
// Special thanks to Toxic, Forever and Weatherbee on WXForum for their assistance making the script better
//
// Some code provided by Purpleair - converted from Javascript to PHP
// Considered open source.
//
// Provided exclusively to members of wxforum.net
// Tested & written under PHP 7.1.11

// Version 1.1
// * Added Sensor channel B and averaging for ug/m&#179; - thanks to weatherbee
// * Addded timestamp option
// * Added averaging option for AQI values (try and see if it shows more acurately to Purpleair's site when turned on)
// --
// Version 1.2
// * Changed the Average API option to show both channels or to average them (thanks to Forever)
// * Added the One Week reading (thanks to Forever)
// * Added the GetPMComment (thanks to Forever)
// * Updated GetPMComment to the PurpleAir levels as they specify for the messages rather than US Gov standard
// * Did some Error Handling for connections
// --
// Version 1.2a - all changes thanks to Forever
// * Fixed issue with styling missing in Channel B
// * Cleaned up table structure
// * Fixed typos/punctuation
// * Fixed W3 validation errors
// --
// Version 1.3
// * Added option of logging of sensor data to MySQL DB
// * Added PM10 data from Thingspeak
// --
// Version 1.3a
// * Added option to show values of ug/m&#179; in 2 decimal places if value is less than 1 (ex. instead of 0 you will see 0.35 or whatever the value)
// --
// Version 1.3b
// * Fixed SQL issue and issued a Import file
// * Combined HTML
// --
// Version 1.3c
// * Fixed issue with erronous echos and loss of CSS (oops)
// --
// Version 1.4
// * Added color transitions like Purpleair's site to AQI values
// * Fixed CSS missing for non-averaging mode
// * Found and added Channel B to PM10 values. You will need the Thingspeak ID for Channel B - info in thread.
// --
// Version 1.4b
// * Bug fixes
// --
// Version 1.4c
// * Fixed bug with PM10 values
// * Added experinmental PM10 AQI function
// * Tuning of script
// --
// Version 1.5 FINAL RELEASE
// * Fixed bug where if decimal point was turned off decimal point numbers would still show - now display 0 as originally intended
// * Added option to cache results - this is useful if you have a high traffic site. PA doesn't like you doing many calls to it.
// * Removed the site up checking, it slows the script and is redundant since the script will return an error anyways if it fails.
// * Changed m/3 to proper HTML entity (thanks Toxic)
// * Added "Very Low" value to GetPMMessage for values < 1.0
// * Added additional levels to GetPMComment
// * Added GetAQIExMessage - returns extended EPA recommendations for the AQI passed to it
// * Added options to turn on/off the Title and Timestamps

// ================================================================================
// BEGIN CONFIGURABLE OPTIONS HERE
// ================================================================================

// --------------------------
// INSERT YOUR SENSOR IDS HERE - THIS IS FOR BOTH CHANNEL A AND B. See thread for info.
$sensorida = "29935"; // for SSV 1D
$sensoridb = "29936";

// INSERT YOUR THINGSPEAK API KEYS HERE FOR PM10 RESULTS. See thread for info, but can be found in your sensor's JSON file.
$TSID = "748775";
$TSKey = "YLG6TNC9653D25C5";
$TSID2 = "748779";
$TSKey2 = "3EMRGGM0OEILEY41";

// --------------------------
// USE COLOR TRANSITIONS (SHADING) LIKE PURPLEAIR DOES AS VALUE GETS NEARER TO NEXT LEVEL (1) OR JUST USE STATIC COLOR TO COLOR (0)
$useblending = 1;
// --------------------------
// SHOW TIMESTAMP OF SENSOR DATA - 1= YES 0=NO
$showtimestamp = 1;
// TIMEZONE MUST BE IN FORMAT PHP RECOGNIZES - SEE PHP MANUALS IF YOU NEED HELP
$timezone = "America/Los_Angeles";
// AVERAGE THE AQI VALUES BETWEEN CHANNEL A AND B IN ADDITION TO THE PM VALUES (1), OR SHOW BOTH SENSORS INDIVIDUALLY (0)
$averageaqi = 1;
// SHOW ug/m3 RESULTS TO 2 DECIMAL PLACES INSTEAD OF JUST 0 IF LESS THAN 0.1
$showdecimal = 1;
// INSERT YOUR LOCATION OR SENSOR NAME
$showtitle = 1;
$sensorname = "SSV 1D";
// DISPLAY PM10 PARTICLE VALUES
// Note: You can display the PM10 AQI value by using the variable $AQI10. Put it in some HTML using the CSS class AQI10 as well.
$showpm10 = 1;
// CACHE RESULTS
// This will cache the results from sensor for the set time period. Once it is over, the data will be re-requested from PA/TS.
// You should turn this ON if you have a high traffic site. 1 = ON, 0 = OFF
// IMPORTANT: The script & directory needs to have WRITE permissions in the directory it is in for this to work.
$cachedata = 0;
$cacheseconds = 180;	//60 = 1 min, 300 = 5 min
// Get All Sensor VALUES
// If you want all of the sensor values to be retrieved, enable this option. The script will take much more time to load since it has to retrieve more data.
// It is recommended you Cache and CRON the script ocassionally otherwise visitors will have to wait about 15-20 seconds for the data to load.
// This also requires the THINGSPEAK_SECONDARY_ID values from the PA JSON file. See thread for details.

$getalldata = 1;
$TSSecID = '748777';
$TSSecID2 = '748781';
$TSSecKey = 'VZTH0O9N2I5LAGSK';
$TSSecKey2 = 'ZOADT99G9ITR5JB1';
// --------------------------
// SQL LOGGING MODE
// 0 = DON'T LOG ANY RESULTS; JUST DISPLAYS THE PURPLEAIR RESULTS AS NORMAL
// 1 = LOG THE RESULTS TO DATABASE AND DISPLAY THE PURPLEAIR RESULTS AS NORMAL
// 2 = LOG THE RESULTS TO DATABASE AND DON'T DISPLAY ANYTHING (CRON-TYPE SCRIPT)
//
// Note: The AQI logged results are always combined values (A+B) like what is shown on Purpleairs site. You can change the script if you want it done differently.
// PM values are logged as each channel.
//
// IMPORTANT: YOU MUST CREATE THE DATABASE AND ITS LAYOUT - A MYSQL EXPORT IS AVAILABLE IN THE THREAD.
// IMPORTANT: IT IS NOT RECOMMENDED TO USE THIS WHEN USING CACHING MODE, YOU WILL JUST GET REPEATED VALUES OF THE CACHE.
$logtosql = 1;
// --------------------------
//MYSQL PARAMETERS FOR SQL LOGGING MODES
$servername = "localhost";
$username = "drewc"; // (on Drew's Macbook)
$password = "rivadog8";
$dbname = "purpleair";
$tbldata = "padata";

// END OF CONFIGURABLE OPTIONS
// ----------------------------------------------------------------------------

if ($_GET['version'] == 1) {
	echo "genrel-1.5";
	die;
}

if ($sensorida == "")
{
	echo "No sensor ID A entered in Script source!";
	die;
}

if ($sensoridb == "")
{
	echo "No sensor ID B entered in Script source!";
	die;
}

if ($TSID == "" || $TSID2 == "" || $TSKey == "" || $TSKey2 == "")
{
	echo "No Thingspeak credentials entered.";
	die;
}

if ($getalldata == 1) {
	if ($TSSecID == "" || $TSSecID2 == "" || $TSSecKey == "" || $TSSecKey2 == "")
	{
		echo "No Secondary Thingspeak credentials entered.";
		die;
	}
}

// Set some of our variables as Globals otherwise PHP throws Info warnings

global $pajsona;
global $pajsonb;
global $pm10;
global $pm102;
global $json_resulta;
global $json_resultb;
global $result2;
global $result3;
global $result4;
global $result5;
global $AQIa;
global $AQIb;
global $AQI;
global $AQIshortterma;
global $AQIshorttermb;
global $AQIshortterm1;
global $AQIshortterm2;
global $AQIrealtime1;
global $AQIrealtime2;
global $AQIshortterma;
global $AQIshorttermb;
global $AQIshortterm;
global $AQI301;
global $AQI302;
global $AQI601;
global $AQI602;
global $AQI6h1;
global $AQI6h2;
global $AQI24h1;
global $AQI24h2;
global $AQI1w1;
global $AQI1w2;
global $pm1;
global $pm12;
global $parts031;
global $parts032;
global $parts051;
global $parts052;
global $parts101;
global $parts102;
global $parts251;
global $parts252;
global $parts501;
global $parts502;
global $parts1001;
global $parts1002;
global $lasertemp;
global $laserhum;
global $extrasensors;
	

if ($cachedata == 1) {
	
	$file = __DIR__ . "/pacache1.txt";
	$file2 = __DIR__ . "/pacache2.txt";
	$file3 = __DIR__ . "/tscache1.txt";
	$file4 = __DIR__ . "/tscache2.txt";
	$file5 = __DIR__ . "/tscachex.txt";

	$filemtime = @filemtime($file);  // returns FALSE if file does not exist
	if ($filemtime == FALSE) {
	//Get the sensor data via JSON
		$pajsona = file_get_contents("http://www.purpleair.com/json?show=" . $sensorida);
		$pajsonb = file_get_contents("http://www.purpleair.com/json?show=" . $sensoridb);

		//Get the PM10 value from Thingspeak
		$pm10 = file_get_contents("https://api.thingspeak.com/channels/".$TSID."/fields/field3/last.txt?api_key=" . $TSKey);
		$pm102 = file_get_contents("https://api.thingspeak.com/channels/".$TSID2."/fields/field3/last.txt?api_key=" . $TSKey2);
		if ($getalldata == 1) {
			LoadAllData();
		}
		$json_resulta=json_decode($pajsona);
		$json_resultb=json_decode($pajsonb);
		$result2 = $json_resulta->results[0]->Stats;
		$result3 = json_decode($result2);
		$result4 = $json_resultb->results[0]->Stats;
		$result5 = json_decode($result4);
		try {
			file_put_contents($file, $pajsona, LOCK_EX);
			file_put_contents($file2, $pajsonb, LOCK_EX);
			file_put_contents($file3, $pm10, LOCK_EX);
			file_put_contents($file4, $pm102, LOCK_EX);
			if ($getalldata == 1)
			{
				$wfile = fopen($file5,"w");
				for($i=0; $i<count($extrasensors); $i++) {
  					$data=array($extrasensors[$i]);
  					fputcsv($wfile, $data);
			}
			fclose($file);
			}
		} catch (Exception $e) {
			echo("Error writing the cache files. Make sure they have 777 (Write) permissions.");
			error_log("Could not write the cache files. Error was: ", $e->getMessage(), "\n");
			die;
		}
	}
	if (!$filemtime or (time() - $filemtime >= $cacheseconds)){
		//Get the sensor data via JSON
		$pajsona = file_get_contents("http://www.purpleair.com/json?show=" . $sensorida);
		$pajsonb = file_get_contents("http://www.purpleair.com/json?show=" . $sensoridb);
		
		if ($getalldata == 1) {
			LoadAllData();
		}

		//Get the PM10 value from Thingspeak
		$pm10 = file_get_contents("https://api.thingspeak.com/channels/".$TSID."/fields/field3/last.txt?api_key=" . $TSKey);
		$pm102 = file_get_contents("https://api.thingspeak.com/channels/".$TSID2."/fields/field3/last.txt?api_key=" . $TSKey2);
		$json_resulta=json_decode($pajsona);
		$json_resultb=json_decode($pajsonb);
		$result2 = $json_resulta->results[0]->Stats;
		$result3 = json_decode($result2);
		$result4 = $json_resultb->results[0]->Stats;
		$result5 = json_decode($result4);
		try {
			file_put_contents($file, $pajsona, LOCK_EX);
			file_put_contents($file2, $pajsonb, LOCK_EX);
			file_put_contents($file3, $pm10, LOCK_EX);
			file_put_contents($file4, $pm102, LOCK_EX);
			if ($getalldata == 1)
			{
				$wfile = fopen($file5,"w");
				for($i=0; $i<count($extrasensors); $i++) {
  					$data=array($extrasensors[$i]);
  					fputcsv($wfile, $data);
			}
			fclose($file);
			}
		} catch (Exception $e) {
			echo("Error writing the cache files. Make sure they have 777 (Write) permissions.");
			error_log("Could not write the cache files. Error was: ", $e->getMessage(), "\n");
			die;
		}
	} else {
		if ($getalldata == 1) {
			$lines = file("tscachex.txt");
			$pm1 = $lines[0];
			$pm12 = $lines[1];
			$parts031 = $lines[2];
			$parts032 = $lines[3];
 			$parts051 = $lines[4];
			$parts052 = $lines[5];
			$parts101 = $lines[6];
			$parts102 = $lines[7];
			$parts251 = $lines[8];
			$parts252 = $lines[9];
			$parts501 = $lines[10];
			$parts502 = $lines[11];
			$parts1001 = $lines[12];
			$parts1002 = $lines[13];
			$lasertemp = $lines[14];
			$laserhum = $lines[15];
		}
		$pajsona = file_get_contents($file);
		$pajsonb = file_get_contents($file2);
		$pm10 = file_get_contents($file3);
		$pm102 = file_get_contents($file4);
		$json_resulta=json_decode($pajsona);
		$json_resultb=json_decode($pajsonb);
		$result2 = $json_resulta->results[0]->Stats;
		$result3 = json_decode($result2);
		$result4 = $json_resultb->results[0]->Stats;
		$result5 = json_decode($result4);
}
} else {
			//Get the sensor data via JSON
		$pajsona = file_get_contents("http://www.purpleair.com/json?show=" . $sensorida);
		$pajsonb = file_get_contents("http://www.purpleair.com/json?show=" . $sensoridb);
		
		if ($getalldata == 1) {
			LoadAllData();
		}

		//Get the PM10 value from Thingspeak
		$pm10 = file_get_contents("https://api.thingspeak.com/channels/".$TSID."/fields/field3/last.txt?api_key=" . $TSKey);
		$pm102 = file_get_contents("https://api.thingspeak.com/channels/".$TSID2."/fields/field3/last.txt?api_key=" . $TSKey2);
		$json_resulta=json_decode($pajsona);
		$json_resultb=json_decode($pajsonb);
		$result2 = $json_resulta->results[0]->Stats;
		$result3 = json_decode($result2);
		$result4 = $json_resultb->results[0]->Stats;
		$result5 = json_decode($result4);
}


if ($pajsona == "" or $pajsonb == "")
{
	echo "No Air Quality data is currently available for this location - Retrieval Error.";
	if ($pajsona == "") {
		$sensorbad = "A";
	} elseif ($pajsonb == "") {
		$sensorbad = "B";
	} elseif ($pajson == "" || $pajsonb == "") {
		$sensorbad = "A and B";
	}
	error_log("No data from Purpleair servers for the script to process (Sensor $sensorbad). Possibly PA site is down or incorrect sensor IDs.", 0);
	die;
}

if (isset($TSID) == true && isset($TSID2) == true) {
if ($pm10 == "" or $pm102 == "")
{
	echo "No Air Quality data is currently available for this location - Retrieval Error.";
	if ($pm10 == "") {
		$sensorbad2 = "A";
	} elseif ($pm102 == "") {
		$sensorbad2 = "B";
	} elseif ($pm10 == "" || $pm102 == "") {
		$sensorbad2 = "A and B";
	}
	error_log("No data from Thingspeak servers for the script to process (Sensor $sensorbad2). Possibly TS site is down or bad IDs/access keys.", 0);
	die;
}
}


//Create our variables for different Sensor results
$aq1 = $result3->v;
$aqst1 = $result3->v1;
$aq301 = $result3->v2;
$aq601 = $result3->v3;
$aq6h1 = $result3->v4;
$aq24h1 = $result3->v5;
$aq1w1 = $result3->v6;
$aqpm1 = $result3->pm;
$timestamp = $result3->lastModified;
$aq2 = $result5->v;
$aqst2 = $result5->v1;
$aq302 = $result5->v2;
$aq602 = $result5->v3;
$aq6h2 = $result5->v4;
$aq24h2 = $result5->v5;
$aq1w2 = $result5->v6;
$aqpm2 = $result5->pm;

//Add the sensor results together (A+B)

if ($showdecimal == 1) {
	if ($aq1 + $aq2 < "1.0") {
	$aq = round(($aq1 + $aq2)/2,2);
} else {
	$aq = round(($aq1 + $aq2)/2,0);

}
if ($aqst1 + $aqst2 < "1.0") {
	$aqst = round(($aqst1 + $aqst2)/2,2);
} else {
	$aqst = round(($aqst1 + $aqst2)/2,0);

}
if ($aq301 + $aq302 < "1.0") {
	$aq30 = round(($aq301 + $aq302)/2,2);
} else {
	$aq30 = round(($aq301 + $aq302)/2,0);

}
if ($aq601 + $aq602 < "1.0") {
	$aq60 = round(($aq601 + $aq602)/2,2);
} else {
	$aq60 = round(($aq601 + $aq602)/2,0);

}
if ($aq6h1 + $aq6h2 < "1.0") {
	$aq6h = round(($aq6h1 + $aq6h2)/2,2);
} else {
	$aq6h = round(($aq6h1 + $aq6h2)/2,0);

}
if ($aq24h1 + $aq24h2 < "1.0") {
	$aq24h = round(($aq24h1 + $aq24h2)/2,2);
} else {
	$aq24h = round(($aq24h1 + $aq24h2)/2,0);

}
if ($aq1w1 + $aq1w2 < "1.0") {
	$aq1w = round(($aq1w1 + $aq1w2 )/2,2);
} else {
	$aq1w = round(($aq1w1 + $aq1w2 )/2,0);

}
if ($pm10 + $pm102 < "1.0") {
	$pm10val = round(($pm10 + $pm102)/2,2);
} else {
	$pm10val = round(($pm10 + $pm102)/2,0);
}
} else {
	if ($aq1 + $aq2 < "1.0") {
		$aq = 0;
	} else {
		$aq = round(($aq1 + $aq2)/2,0);
	}
	if ($aqst + $aqst2 < "1.0") {
		$aqst = 0;
	} else {
		$aqst = round(($aqst1 + $aqst2)/2,0);
	}
	if ($aq301 + $aq302 < "1.0") {
		$aq30 = 0;
	} else {
		$aq30 = round(($aq301 + $aq302)/2,0);
	}
	if ($aq601 + $aq602 < "1.0") {
		$aq60 = 0;
	} else {
		$aq60 = round(($aq601 + $aq602)/2,0);
	}
	if ($aq6h1 + $aq6h2 < "1.0") {
		$aq6h = 0;
	} else {
		$aq6h = round(($aq6h1 + $aq6h2)/2,0);
	}
	if ($aq24h1 + $aq24h2 < "1.0") {
		$aq24h = 0;
	} else {
		$aq24h = round(($aq24h1 + $aq24h2)/2,0);
	}
	if ($aq1w1 + $aq1w2 < "1.0") {
		$aq1w = 0;
	} else {
		$aq1w = round(($aq1w1 + $aq1w2)/2,0);
	}
	if ($pm10 + $pm102 < "1.0") {
		$pm10val = 0;
	} else {
		$pm10val = round(($pm10 + $pm102)/2,0);
	}
}

//Set the variables and get AQI from PPM
if ($averageaqi == 1) {
$AQIa = aqiFromPM($aqst1);
$AQIb = aqiFromPM($aqst2);
$AQI = round(($AQIa + $AQIb) / 2);
$AQIrealtimea = aqiFromPM($aq1);
$AQIrealtimeb = aqiFromPM($aq2);
$AQIrealtime = round(($AQIrealtimea + $AQIrealtimeb) / 2);
$AQIshortterma = aqiFromPM($aqst1);
$AQIshorttermb = aqiFromPM($aqst2);
$AQIshortterm = round(($AQIshortterma + $AQIshorttermb) / 2);
$AQI30a = aqiFromPM($aq301);
$AQI30b = aqiFromPM($aq302);
$AQI30 = round(($AQI30a + $AQI30b) / 2);
$AQI60a = aqiFromPM($aq601);
$AQI60b = aqiFromPM($aq602);
$AQI60 = round(($AQI60a + $AQI60b) / 2);
$AQI6ha = aqiFromPM($aq6h1);
$AQI6hb = aqiFromPM($aq6h2);
$AQI6h = round(($AQI6ha + $AQI6hb) / 2);
$AQI24ha = aqiFromPM($aq24h1);
$AQI24hb = aqiFromPM($aq24h2);
$AQI24h = round(($AQI24ha + $AQI24hb) / 2);
$AQI1wa = aqiFromPM($aq1w1);
$AQI1wb = aqiFromPM($aq1w2);
$AQI1w = round(($AQI1wa + $AQI1wb) / 2);

if (isset($TSID) == true && isset($TSID2) == true) {
	$AQI10a = aqiFromPM($pm10);
	$AQI10b = aqiFromPM($pm102);
	$AQI10 = round(($AQI10a + $AQI10b) / 2);
}

} else {
	$AQIa = aqiFromPM($aqst1);
	$AQIb = aqiFromPM($aqst2);
	$AQI = round(($AQIa + $AQIb) / 2);
	$AQIshortterma = aqiFromPM($aqst1);
	$AQIshorttermb = aqiFromPM($aqst2);
	$AQIshortterm = round(($AQIshortterma + $AQIshorttermb) / 2);
	$AQIrealtime1 = aqiFromPM($aq1);
	$AQIshortterm1 = aqiFromPM($aqst1);
	$AQI301 = aqiFromPM($aq301);
	$AQI601 = aqiFromPM($aq601);
	$AQI6h1 = aqiFromPM($aq6h1);
	$AQI24h1 = aqiFromPM($aq24h1);
	$AQI1w1 = aqiFromPM($aq1w1);
	$AQIrealtime2 = aqiFromPM($aq2);
	$AQIshortterm2 = aqiFromPM($aqst2);
	$AQI302 = aqiFromPM($aq302);
	$AQI602 = aqiFromPM($aq602);
	$AQI6h2 = aqiFromPM($aq6h2);
	$AQI24h2 = aqiFromPM($aq24h2);
	$AQI1w2 = aqiFromPM($aq1w2);
if (isset($TSID) == true && isset($TSID2) == true) {
	$AQI10a = aqiFromPM($pm10);
	$AQI10b = aqiFromPM($pm102);
	$AQI10 = round(($AQI10a + $AQI10b) / 2);
}
}

$Description = getAQIDescription($AQI);
$Message = GetAQIMessage($AQI);
$MessageComment = GetPMComment($aqst);

if ($showtimestamp == 1) {
date_default_timezone_set($timezone);
$sensorstamp = date('D, M. d, Y -  h:i a', $timestamp/1000);
}

//Timestamp for SQL
$timestamp = date("Y-m-d H:i:s");

if ($logtosql == 2) {

// Create connection
$conn = new mysqli($servername, $username, $password, $dbname);
// Check connection
if ($conn->connect_error) {
    die("Connection failed: " . $conn->connect_error);
} 

$sql = "INSERT INTO $tbldata (ID, time, aqivalue, rtaqi, staqi, 30minaqi, 1hraqi, 6hraqi, 24hraqi, 1wkaqi, chartgms, chastgms,
cha30mingms,cha1hrgms,cha6hrgms,cha24hrgms,cha1wkgms,chbrtgms,chbstgms,chb30mingms,chb1hrgms,chb6hrgms,chb24hrgms,chb1wgms)
VALUES (NULL, '$timestamp', '$AQI', '$AQIrealtime', '$AQIshortterm', '$AQI30', '$AQI60', '$AQI6h', '$AQI24h', '$AQI1w', '$aq1', '$aqst1', '$aq301', '$aq601', '$aq6h1', '$aq24h1', '$aq1w1', '$aq2', '$aqst2', '$aq302', '$aq602', '$aq6h2', '$aq24h2', '$aq1w2')";


if ($conn->query($sql) === FALSE) {
    echo "Error inserting SQL for Purpleair Script: " . $sql . "<br>" . $conn->error;
	die;
}

$conn->close();
die;
}

global $AQIrealtime;
global $AQI30;
global $AQI60;
global $AQI6h;
global $AQI24h;
global $AQI1w;

echo '
<style type="text/css">
.auto-style1 {
	font-size: xx-large;
}

.auto-style8 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI)[0] . ';
	color: ' . GetColor($AQI)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	font-size: 36pt;
}
';

if (isset($TSID) == true && isset($TSID2) == true) {
	if($showpm10 == 1) {
echo '
.AQI10 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI10)[0] . ';
	color: ' . GetColor($AQI10)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	font-size: 36pt;
}';
	}
}
echo '
.rtitems {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQIrealtime)[0] . ';
	color: ' . GetColor($AQIrealtime)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}

.rtitems1 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQIrealtime1)[0] . ';
	color: ' . GetColor($AQIrealtime1)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}
	
.rtitems2 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQIrealtime2)[0] . ';
	color: ' . GetColor($AQIrealtime2)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}
	
.stitems {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQIshortterm)[0] . ';
	color: ' . GetColor($AQIshortterm)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}

.stitems1 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQIshortterm1)[0] . ';
	color: ' . GetColor($AQIshortterm1)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}

.stitems2 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQIshortterm2)[0] . ';
	color: ' . GetColor($AQIshortterm2)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}	

.thirtyitems {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI30)[0] . ';
	color: ' . GetColor($AQI30)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}

.thirtyitems1 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI301)[0] . ';
	color: ' . GetColor($AQI301)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}
	
.thirtyitems2 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI302)[0] . ';
	color: ' . GetColor($AQI302)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}

.sixtyitems {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI60)[0] . ';
	color: ' . GetColor($AQI60)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}
	
.sixtyitems1 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI601)[0] . ';
	color: ' . GetColor($AQI601)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}
	
.sixtyitems2 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI602)[0] . ';
	color: ' . GetColor($AQI602)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}

.sixhouritems {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI6h)[0] . ';
	color: ' . GetColor($AQI6h)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}
	
.sixhouritems1 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI6h1)[0] . ';
	color: ' . GetColor($AQI6h1)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}

.sixhouritems2 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI6h2)[0] . ';
	color: ' . GetColor($AQI6h2)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}

.twentyfouritems {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI24h)[0] . ';
	color: ' . GetColor($AQI24h)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}
	
.twentyfouritems1 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI24h1)[0] . ';
	color: ' . GetColor($AQI24h1)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}


.twentyfouritems2 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI24h2)[0] . ';
	color: ' . GetColor($AQI24h2)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}

.oneweekitems {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI1w)[0] . ';
	color: ' . GetColor($AQI1w)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}
	
.oneweekitems1 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI1w1)[0] . ';
	color: ' . GetColor($AQI1w1)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}

.oneweekitems2 {
	margin: 0 0 5px;
	overflow: hidden;
	padding: 5px;
	background-color: ' . GetColor($AQI1w2)[0] . ';
	color: ' . GetColor($AQI1w2)[1] . ';
	border: 1px solid #afcde3;
	-webkit-border-radius: 5px;
	border-radius: 5px;
	text-align: center;
	font-family: Arial, Helvetica, sans-serif;
	}


.auto-style9 {
	font-family: Arial, Helvetica, sans-serif;
}

.auto-style10 {
	font-family: Arial, Helvetica, sans-serif;
	text-align: center;
	padding: 10px;
}

.auto-style11 {
	font-size: x-small;
}

.auto-style12 {
	font-size: medium;
}

.auto-style13 {
	font-family: Arial, Helvetica, sans-serif;
	font-size: small;
}

.auto-style14 {
	font-family: Arial, Helvetica, sans-serif;
	text-align: center;
	font-size: x-small;
}

.auto-style15 {
	font-family: Arial, Helvetica, sans-serif;
	font-size: large;
}

</style>
		
<table style="width: 100%">
		<tr>
			<th class="auto-style9"><h2>Air Quality Index</h2></th>
		</tr>';
if ($showtitle == 1);
{
	echo '<tr>
			<th class="auto-style9"><h2>' . $sensorname . '</h2></th>
		  </tr>';
}
if ($showtimestamp == 1);
{
	echo '
		<tr>
			<th class="auto-style10">' . $sensorstamp . '</th>
		</tr>';
}
echo '
		<tr>
			<th style="width: 100%"><span class="auto-style8">' . $AQIshortterm . '</span></th>
		</tr>
		<tr>
			<th class="auto-style15" style="width: 100%" font-size="large"><strong>' . $Description . '</strong></th>
		</tr>
';

if ($averageaqi == 1) {	
echo '	<table style="width: 100%">
		<tr>
			<td colspan="7" style="height: 28px" class="auto-style13">' . $Message . '</td>
		</tr>
			<td colspan="7" style="height: 28px" class="auto-style13"><b>Running Averages of Channel A and B</b></td>
		<tr>
			<td class="auto-style10" style="width: 14%"><p class="rtitems">
			<span class="auto-style12">' . $AQIrealtime . '</span><br>
			<span class="auto-style11">' . $aq . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="stitems">
			<span class="auto-style12">' . $AQIshortterm . '</span><br>
			<span class="auto-style11">' . $aqst . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="thirtyitems">
			<span class="auto-style12">' . $AQI30 . '</span><br>
			<span class="auto-style11">' . $aq30 . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="sixtyitems">
			<span class="auto-style12">' . $AQI60 . '</span><br>
			<span class="auto-style11">' . $aq60 . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="sixhouritems">
			<span class="auto-style12">' . $AQI6h . '</span><br>
			<span class="auto-style11">' . $aq6h . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="twentyfouritems">
			<span class="auto-style12">' . $AQI24h . '</span><br>
			<span class="auto-style11">' . $aq24h . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="oneweekitems">
			<span class="auto-style12">' . $AQI1w . '</span><br>
			<span class="auto-style11">' . $aq1w . ' &#181;g/m&#179;</span></p>
			</td>
		</tr>
		<tr>
			<td class="auto-style14" style="width: 14%"><strong>Real-Time</strong></td>
			<td class="auto-style14" style="width: 14%"><strong>Short Term</strong></td>
			<td class="auto-style14" style="width: 14%"><strong>30 Minutes</strong></td>
			<td class="auto-style14" style="width: 14%"><strong>1 Hour</strong></td>
			<td class="auto-style14" style="width: 14%"><strong>6 Hours</strong></td>
			<td class="auto-style14" style="width: 14%" ><strong>24 Hours</strong></td>
			<td class="auto-style14" style="width: 14%" ><strong>One Week</strong></td>
		</tr>

		<tr>
		<td colspan="7" style="height: 48px" class="auto-style13">Short-term PM 2.5 is '. GetPPMLevel($aqst, "PM25") . ' at ' . $aqst . ' &#181;g/m&#179; - '. $MessageComment . '</td>	</tr>	
';
	if ($showpm10 == 1) {
		if (empty($TSID)) {
			error_log("Cannot show PM10 when there is no IDs");
		} else {
		echo '
		<tr><td colspan="7" style="height: 48px" class="auto-style13">Short-term PM 10 is '. GetPPMLevel($pm10val, "PM10") . ' at ' . $pm10val . ' &#181;g/m&#179; </td>	';
		}
	}
echo '		
		</tr>
	</table>
	
</table>

';

} else {
echo '	<table style="width: 100%">
		<tr>
			<td colspan="7" style="height: 28px" class="auto-style13">' . $Message . '</td>
		</tr>
			<td colspan="7" style="height: 28px" class="auto-style13"><b>Channel A Running Averages</b></td>
		<tr>
			<td class="auto-style10" style="width: 14%"><p class="rtitems1">
			<span class="auto-style12">' . $AQIrealtime1 . '</span><br>
			<span class="auto-style11">' . $aq1 . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="stitems1">
			<span class="auto-style12">' . $AQIshortterm1 . '</span><br>
			<span class="auto-style11">' . $aqst1 . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="thirtyitems1">
			<span class="auto-style12">' . $AQI301 . '</span><br>
			<span class="auto-style11">' . $aq301 . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="sixtyitems1">
			<span class="auto-style12">' . $AQI601 . '</span><br>
			<span class="auto-style11">' . $aq601 . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="sixhouritems1">
			<span class="auto-style12">' . $AQI6h1 . '</span><br>
			<span class="auto-style11">' . $aq6h1 . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="twentyfouritems1">
			<span class="auto-style12">' . $AQI24h1 . '</span><br>
			<span class="auto-style11">' . $aq24h1 . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="oneweekitems1">
			<span class="auto-style12">' . $AQI1w1 . '</span><br>
			<span class="auto-style11">' . $aq1w1 . ' &#181;g/m&#179;</span></p>
			</td>
		</tr>
		<tr>
			<td class="auto-style14" style="width: 14%"><strong>Real-Time</strong></td>
			<td class="auto-style14" style="width: 14%"><strong>Short Term</strong></td>
			<td class="auto-style14" style="width: 14%"><strong>30 Minutes</strong></td>
			<td class="auto-style14" style="width: 14%"><strong>1 Hour</strong></td>
			<td class="auto-style14" style="width: 14%"><strong>6 Hours</strong></td>
			<td class="auto-style14" style="width: 14%" ><strong>24 Hours</strong></td>
			<td class="auto-style14" style="width: 14%" ><strong>One Week</strong></td>
		</tr>
			<td colspan="7" style="height: 28px" class="auto-style13"><b>Channel B Running Averages</b></td>
		<tr>
			<td class="auto-style10" style="width: 14%"><p class="rtitems2">
			<span class="auto-style12">' . $AQIrealtime2 . '</span><br>
			<span class="auto-style11">' . $aq2 . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="stitems2">
			<span class="auto-style12">' . $AQIshortterm2 . '</span><br>
			<span class="auto-style11">' . $aqst2 . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="thirtyitems2">
			<span class="auto-style12">' . $AQI302 . '</span><br>
			<span class="auto-style11">' . $aq302 . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="sixtyitems2">
			<span class="auto-style12">' . $AQI602 . '</span><br>
			<span class="auto-style11">' . $aq602 . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="sixhouritems2">
			<span class="auto-style12">' . $AQI6h2 . '</span><br>
			<span class="auto-style11">' . $aq6h2 . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="twentyfouritems2">
			<span class="auto-style12">' . $AQI24h2 . '</span><br>
			<span class="auto-style11">' . $aq24h2 . ' &#181;g/m&#179;</span></p>
			</td>
			<td class="auto-style10" style="width: 14%"><p class="oneweekitems2">
			<span class="auto-style12">' . $AQI1w2 . '</span><br>
			<span class="auto-style11">' . $aq1w2 . ' &#181;g/m&#179;</span></p>
			</td>
		</tr>
		<tr>
			<td class="auto-style14" style="width: 14%"><strong>Real-Time</strong></td>
			<td class="auto-style14" style="width: 14%"><strong>Short Term</strong></td>
			<td class="auto-style14" style="width: 14%"><strong>30 Minutes</strong></td>
			<td class="auto-style14" style="width: 14%"><strong>1 Hour</strong></td>
			<td class="auto-style14" style="width: 14%"><strong>6 Hours</strong></td>
			<td class="auto-style14" style="width: 14%" ><strong>24 Hours</strong></td>
			<td class="auto-style14" style="width: 14%" ><strong>One Week</strong></td>
		</tr>
		<tr>
				<td colspan="7" style="height: 48px" class="auto-style13">Short-term PM<sub>2.5</sub> is '. GetPPMLevel($aqst, "PM25") . ' at ' . $aqst . ' &#181;g/m&#179; - '. $MessageComment . '</td></tr>		';
	if ($showpm10 == 1) {
		if (empty($TSID)) {
			error_log("Cannot show PM10 when there is no IDs");
		} else {
		echo '
		<tr><td colspan="7" style="height: 48px" class="auto-style13">Short-term PM 10 is '. GetPPMLevel($pm10val, "PM10") . ' at ' . $pm10val . ' &#181;g/m&#179; </td>	';
		}
	}
echo '	</tr>
	</table>	
</table>';
}

if ($logtosql == 1) {

// Create connection
$conn = new mysqli($servername, $username, $password, $dbname);
// Check connection
if ($conn->connect_error) {
    die("Connection failed: " . $conn->connect_error);
} 

$sql = "INSERT INTO $tbldata (ID, time, aqivalue, rtaqi, staqi, 30minaqi, 1hraqi, 6hraqi, 24hraqi, 1wkaqi, chartgms, chastgms,
cha30mingms,cha1hrgms,cha6hrgms,cha24hrgms,cha1wkgms,chbrtgms,chbstgms,chb30mingms,chb1hrgms,chb6hrgms,chb24hrgms,chb1wgms)
VALUES (NULL, '$timestamp', '$AQI', '$AQIrealtime', '$AQIshortterm', '$AQI30', '$AQI60', '$AQI6h', '$AQI24h', '$AQI1w', '$aq1', '$aqst1', '$aq301', '$aq601', '$aq6h1', '$aq24h1', '$aq1w1', '$aq2', '$aqst2', '$aq302', '$aq602', '$aq6h2', '$aq24h2', '$aq1w2')";

if ($conn->query($sql) === FALSE) {
    error_log("Error inserting SQL for Purpleair Script: " . $conn->error);
	die;
}

$conn->close();
}

//Function to get AQI number from PPM reading
function aqiFromPM ($pm) {
	if (is_nan($pm)) {
		return "-";
	}
	if (isset($pm) == false) {
		return "Error: No value";
	}
	if ($pm < 0.0) {
		return $pm;
	}
	if ($pm > 1000.0) {
		return "V.HI";
	}
	if ($pm > 350.5) {
		return calcAQI($pm, 500.0, 401.0, 500.0, 350.5);
	} else if ($pm > 250.5) {
    return calcAQI($pm, 400.0, 301.0, 350.4, 250.5);
  } else if ($pm > 150.5) {
    return calcAQI($pm, 300.0, 201.0, 250.4, 150.5);
  } else if ($pm > 55.5) {
    return calcAQI($pm, 200.0, 151.0, 150.4, 55.5);
  } else if ($pm > 35.5) {
    return calcAQI($pm, 150.0, 101.0, 55.4, 35.5);
  } else if ($pm > 12.1) {
    return calcAQI($pm, 100.0, 51.0, 35.4, 12.1);
  } else if ($pm >= 0.0) {
    return calcAQI($pm, 50.0, 0.0, 12.0, 0.0);
  } else {
    return "-";
  }
}

// Test function for calculating AQI of PM10. Unknown if it's correct but appears to be...
function aqiFromPM10 ($pm10num) {
	if (is_nan($pm10num)) {
		return "-";
	}
	if (isset($pm10num) == false) {
		return "Error: No value";
	}
	if ($pm10num < 0.0) {
		return $pm10num;
	}
	if ($pm10num > 1000.0) {
		return "V.HI";
	}
	if ($pm10num > 505) {
		return calcAQI($pm10num, 500.0, 401.0, 604.0, 505.0);
	} else if ($pm10num > 425) {
    return calcAQI($pm10num, 400.0, 301.0, 504.0, 425.0);
  } else if ($pm10num > 355) {
    return calcAQI($pm10num, 300.0, 201.0, 424.0, 355.0);
  } else if ($pm10num > 255) { 
    return calcAQI($pm10num, 200.0, 151.0, 354.0, 255.0);
  } else if ($pm10num > 155) {
    return calcAQI($pm10num, 150.0, 101.0, 254.0, 155.0);
  } else if ($pm10num > 55) {
    return calcAQI($pm10num, 100.0, 51.0, 154.0, 55.0);
  } else if ($pm10num >= 0.0) {
    return calcAQI($pm10num, 50.0, 0.0, 54.0, 0.0);
  } else {
    return "NaN";
  }
}


//Function that actually calculates the AQI number
function calcAQI($Cp, $Ih, $Il, $BPh, $BPl) {
  $a = ($Ih - $Il);
  $b = ($BPh - $BPl);
  $c = ($Cp - $BPl);
  return round(($a/$b) * $c + $Il);     
}

//Function that gets the AQI's description
function getAQIDescription($aqinum) {
  if ($aqinum >= 401) {
  	return 'Hazardous';
  } else if ($aqinum >= 301) {
  	return 'Hazardous';
  } else if ($aqinum >= 201) {
  	return 'Very Unhealthy';
  } else if ($aqinum >= 151) {
  	return 'Unhealthy';
  } else if ($aqinum >= 101) {
  	return 'Unhealthy for Sensitive Groups';
  } else if ($aqinum >= 51) {
  	return 'Moderate';
  } else if ($aqinum >= 0) {
  	return 'Good';
  } else {
  	return 'NaN';
  }
}

//Function that gets the message connected to the AQI
function GetAQIMessage($aqil)
{
  if ($aqil >= 401.0) {
    return ">401: Health alert: everyone may experience more serious health effects.";
  } else if ($aqil >= 301.0) {
    return "301-400: Health alert: everyone may experience more serious health effects.";
  } else if ($aqil >= 201.0) {
    return "201-300: Health warnings of emergency conditions. The entire population is more likely to be affected.";
  } else if ($aqil >= 151.0) {
    return "151-200: Everyone may begin to experience health effects; members of sensitive groups may experience more serious health effects.";
  } else if ($aqil >= 101.0) {
    return "101-150: Members of sensitive groups may experience health effects. The general public is not likely to be affected.";
  } else if ($aqil >= 51.0) {
    return "51-100: Air quality is acceptable; however, for some pollutants there may be a moderate health concern for a very small number of people who are unusually sensitive to air pollution.";
  } else if ($aqil >= 0.0) {
    return "0-50: Air quality is considered satisfactory, and air pollution poses little or no risk.";
  } else {
    return "NaN";
  }
}

// Gets respective color for the AQI
function GetColor($aqinumber)
{
	
global $useblending;

if ($useblending == 1) {
  if ($aqinumber >= 301.0) {
    return array ($color = "#800000", $fcolor = "#FFFFFF");
  } else if ($aqinumber >= 300.0) {
	return array($color = "#80000B", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 299.0) {
	return array($color = "#800017", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 298.0) {
	return array($color = "#800022", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 297.0) {
	return array($color = "#80002E", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 296.0) {
	return array($color = "#80003A", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 295.0) {
	return array($color = "#800045", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 294.0) {
	return array($color = "#800051", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 293.0) {
	return array($color = "#80005D", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 292.0) {
	return array($color = "#800068", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 291.0) {
	return array($color = "#800074", $fcolor = "#000000");
  } else if ($aqinumber >= 201.0) {
    return array ($color = "#800080", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 200.0) {
	return array($color = "#8B0074", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 199.0) {
	return array($color = "#970068", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 198.0) {
	return array($color = "#A2005D", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 197.0) {
	return array($color = "#AE0051", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 196.0) {
	return array($color = "#B90045", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 195.0) {
	return array($color = "#C5003A", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 194.0) {
	return array($color = "#D0002E", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 193.0) {
	return array($color = "#DC0022", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 192.0) {
	return array($color = "#E70017", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 191.0) {
	return array($color = "#F3000B", $fcolor = "#000000");
  } else if ($aqinumber >= 151.0) {
    return array ($color = "#FF0000", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 150.0) {
	return array($color = "#FF0C00", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 149.0) {
	return array($color = "#FF1900", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 148.0) {
	return array($color = "#FF2600", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 147.0) {
	return array($color = "#FF3200", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 146.0) {
	return array($color = "#FF3F00", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 145.0) {
	return array($color = "#FF4C00", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 144.0) {
	return array($color = "#FF5900", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 143.0) {
	return array($color = "#FF6500", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 142.0) {
	return array($color = "#FF7200", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 141.0) {
	return array($color = "#FF7F00", $fcolor = "#000000");
  } else if ($aqinumber >= 101.0) {
    return array ($color = "#FF8C00", $fcolor = "#000000");
  } else if ($aqinumber >= 100.0) {
	return array($color = "#FF9600", $fcolor = "#000000");
  } else if ($aqinumber >= 99.0) {
	return array($color = "#FFA000", $fcolor = "#000000");
  } else if ($aqinumber >= 98.0) {
	return array($color = "#FFAB00", $fcolor = "#000000");
  } else if ($aqinumber >= 97.0) {
	return array($color = "#FFB500", $fcolor = "#000000");
  } else if ($aqinumber >= 96.0) {
	return array($color = "#FFC000", $fcolor = "#000000");
  } else if ($aqinumber >= 95.0) {
	return array($color = "#FFCA00", $fcolor = "#000000");
  } else if ($aqinumber >= 94.0) {
	return array($color = "#FFD500", $fcolor = "#000000");
  } else if ($aqinumber >= 93.0) {
	return array($color = "#FFDF00", $fcolor = "#000000");
  } else if ($aqinumber >= 92.0) {
	return array($color = "#FFEA00", $fcolor = "#000000");
  } else if ($aqinumber >= 91.0) {
	return array($color = "#FFF400", $fcolor = "#000000");
  } else if ($aqinumber >= 51.0) {
    return array($color = "#FFFF00", $fcolor = "#000000");
  } else if ($aqinumber >= 50.0) {
	return array($color = "#E7F300", $fcolor = "#000000");
  } else if ($aqinumber >= 49.0) {
	return array($color = "#D0E700", $fcolor = "#000000");
  } else if ($aqinumber >= 48.0) {
	return array($color = "#B9DC00", $fcolor = "#000000");
  } else if ($aqinumber >= 47.0) {
	return array($color = "#A2D000", $fcolor = "#000000");
  } else if ($aqinumber >= 46.0) {
	return array($color = "#8BC500", $fcolor = "#000000");
  } else if ($aqinumber >= 45.0) {
	return array($color = "#73B900", $fcolor = "#FFFFFF");
  } else if ($aqinumber >= 44.0) {
	return array($color = "#5CAE00", $fcolor = "#FFFFFF");
  } else if ($aqinumber >= 43.0) {
	return array($color = "#45A200", $fcolor = "#FFFFFF");
  } else if ($aqinumber >= 42.0) {
	return array($color = "#2E9700", $fcolor = "#FFFFFF");
  } else if ($aqinumber >= 41.0) {
	return array($color = "#178B00", $fcolor = "#FFFFFF");
  } else if ($aqinumber >= 0.0) {
    return array($color = "#008000", $fcolor = "#FFFFFF");
  } else {
    return array($color = "#FFFFFF", $fcolor = "#000000");
  }
} else {
	if ($aqinumber >= 301.0) {
    return array ($color = "#800000", $fcolor = "#FFFFFF");
  } else if ($aqinumber >= 201.0) {
    return array ($color = "#800080", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 151.0) {
    return array ($color = "#FF0000", $fcolor = "#FFFFFFF");
  } else if ($aqinumber >= 101.0) {
    return array ($color = "#FF8C00", $fcolor = "#000000");
  } else if ($aqinumber >= 51.0) {
    return array($color = "#FFFF00", $fcolor = "#000000");
  } else if ($aqinumber >= 0.0) {
    return array($color = "#008000", $fcolor = "#FFFFFF");
  } else {
    return array($color = "#FFFFFF", $fcolor = "#000000");
  }
}
}

function GetPPMLevel($pmlevel, $type)
{
if ($type == "PM25") {
	if ($pmlevel < 1.0) {
		return "VERY LOW";
	} else if ($pmlevel <= 34.9) {
		return "LOW";
	} else if ($pmlevel >=35) {
		return "MODERATE";
	} else if ($pmlevel >=70) {
		return "HIGH";
	} else if ($pmlevel >=200) {
		return "VERY HIGH";
	} else if ($pmlevel > 500) {
		return "EXTREME";
	} else {
		return "NaN";
	}
}
if ($type == "PM10") {
	if ($pmlevel < 1.0) {
		return "VERY LOW";
	} else if ($pmlevel <= 54) {
		return "LOW";
	} else if ($pmlevel >=55) {
		return "MODERATE";
	} else if ($pmlevel >=155) {
		return "HIGH";
	} else if ($pmlevel >=255) {
		return "VERY HIGH";
	} else if ($pmlevel >= 355) {
		return "EXTREME";
	} else {
		return "NaN";
	}
}
}


function GetPMComment($pmlevel)
{
	if ($pmlevel > 500) {
              return 'Massive source(s) of particle pollution like dust, smoke or exhaust may be nearby. Check the Air Quality Index above to find out if you should adjust activities. NOTE: Very high readings may mean the sensor is not working properly, but doubtful.';
			} else if ($pmlevel > 250) {
			  return 'Very large sized source(s) of particle pollution like dust, smoke or exhaust may be nearby. Check the Air Quality Index above to plan activities.';
			} else if ($pmlevel > 100) {
			  return 'Large size source(s) of particle pollution like dust, smoke or exhaust may be nearby. Check the Air Quality Index above to plan activities.';
            } else if ($pmlevel > 70) {
              return 'Medium size source(s) of particle pollution like dust, smoke or exhaust may be nearby. Check the Air Quality Index above to plan activities.';
            } else if ($pmlevel > 30) {
              return 'Small sized source(s) of particle pollution like dust, smoke or exhaust may be nearby. If moderate readings continue (for an hour or more), use the Air Quality Index above to plan activities.';
            } else if ($pmlevel >= 0) {
              return 'No unusual sources of pollution present at this time.';
            } else {
              return "NaN";
            }
}

// Extended AQI Message direct from EPA Website

function GetAQIExMessage($plevel)
{
	$sensitivegroups = 'People with respiratory or heart disease, the elderly and children are the groups most at risk.';
	
  if ($plevel >= 301.0) {
    return array($sensitivegroups, $healtheffects="Serious aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly; serious risk of respiratory effects in general population.", $cautionary="Everyone should avoid any outdoor exertion; people with respiratory or heart disease, the elderly and children should remain indoors.");
  } else if ($plevel >= 201.0) {
    return array($sensitivegroups, $healtheffects="Significant aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly; significant increase in respiratory effects in general population.", $cautionary="People with respiratory or heart disease, the elderly and children should avoid any outdoor activity; everyone else should avoid prolonged exertion.");
  } else if ($plevel >= 151.0) {
    return array($sensitivegroups, $healtheffects="Increased aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly; increased respiratory effects in general population.", $cautionary="People with respiratory or heart disease, the elderly and children should avoid prolonged exertion; everyone else should limit prolonged exertion.");
  } else if ($plevel >= 101.0) {
    return array($sensitivegroups, $healtheffects="Increasing likelihood of respiratory symptoms in sensitive individuals, aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly.", $cautionary="People with respiratory or heart disease, the elderly and children should limit prolonged exertion.");
  } else if ($plevel >= 51.0) {
    return array($sensitivegroups, $healtheffects="Unusually sensitive people should consider reducing prolonged or heavy exertion.", $cautionary="Unusually sensitive people should consider reducing prolonged or heavy exertion.");
  } else if ($plevel >= 0.0) {
    return array($sensitivegroups, $healtheffects="None.", $cautionary="None.");
  } else {
    return "NaN";
  }
}

function LoadAllData()
{

global $TSID;
global $TSID2;
global $TSKey;
global $TSKey2;
global $TSSecID;
global $TSSecID2;
global $TSSecKey;
global $TSSecKey2;
global $pm1;
global $pm12;
global $parts031;
global $parts032;
global $parts051;
global $parts052;
global $parts101;
global $parts102;
global $parts251;
global $parts252;
global $parts501;
global $parts502;
global $parts1001;
global $parts1002;
global $lasertemp;
global $laserhum;
global $extrasensors;
			$pm1 = file_get_contents("https://api.thingspeak.com/channels/".$TSID."/fields/field1/last.txt?api_key=" . $TSKey);
			$pm12 = file_get_contents("https://api.thingspeak.com/channels/".$TSID2."/fields/field1/last.txt?api_key=" . $TSKey2);
			$parts031 = file_get_contents("https://api.thingspeak.com/channels/".$TSSecID."/fields/field1/last.txt?api_key=" . $TSSecKey);	// 0.3 particles count
			$parts032 = file_get_contents("https://api.thingspeak.com/channels/".$TSSecID2."/fields/field1/last.txt?api_key=" . $TSSecKey2);	
 			$parts051 = file_get_contents("https://api.thingspeak.com/channels/".$TSSecID."/fields/field2/last.txt?api_key=" . $TSSecKey);	// 0.5 particles count
			$parts052 = file_get_contents("https://api.thingspeak.com/channels/".$TSSecID2."/fields/field2/last.txt?api_key=" . $TSSecKey2);
			$parts101 = file_get_contents("https://api.thingspeak.com/channels/".$TSSecID."/fields/field3/last.txt?api_key=" . $TSSecKey);	// 1.0 particles count
			$parts102 = file_get_contents("https://api.thingspeak.com/channels/".$TSSecID2."/fields/field3/last.txt?api_key=" . $TSSecKey2);
			$parts251 = file_get_contents("https://api.thingspeak.com/channels/".$TSSecID."/fields/field4/last.txt?api_key=" . $TSSecKey);			// 2.5 particles count
			$parts252 = file_get_contents("https://api.thingspeak.com/channels/".$TSSecID2."/fields/field4/last.txt?api_key=" . $TSSecKey2);
			$parts501 = file_get_contents("https://api.thingspeak.com/channels/".$TSSecID."/fields/field5/last.txt?api_key=" . $TSSecKey);			// 5.0 particles count
			$parts502 = file_get_contents("https://api.thingspeak.com/channels/".$TSSecID2."/fields/field5/last.txt?api_key=" . $TSSecKey2);
			$parts1001 = file_get_contents("https://api.thingspeak.com/channels/".$TSSecID."/fields/field6/last.txt?api_key=" . $TSSecKey);			// 10.0 particles count
			$parts1002 = file_get_contents("https://api.thingspeak.com/channels/".$TSSecID2."/fields/field6/last.txt?api_key=" . $TSSecKey2);
			$lasertemp = file_get_contents("https://api.thingspeak.com/channels/".$TSID."/fields/field6/last.txt?api_key=" . $TSKey);			// 10.0 particles count
			$laserhum = file_get_contents("https://api.thingspeak.com/channels/".$TSID."/fields/field7/last.txt?api_key=" . $TSKey);
			$extrasensors = array($pm1,$pm12,$parts031,$parts032,$parts051,$parts052,$parts101,$parts102,$parts251,$parts252,$parts501,$parts502,$parts1001,$parts1002,$lasertemp,$laserhum);
}

?>