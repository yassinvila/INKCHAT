//MTA PROXY 
import express from "express";
import cors from "cors";
import GtfsRealtimeBindings from "gtfs-realtime-bindings";

const northA = "A28N";
const southA = "A28S";
const northN = "R17N";
const southN = "R17S";
const MTA_URL_A =   
    "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-ace";
const MTA_URL_N = 
    "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-nqrw"


const LAT = 40.7506;
const LON = -73.9935;
const WEATHER_URL = 
    "https://api.open-meteo.com/v1/forecast?latitude=40.7506&longitude=-73.9935&daily=temperature_2m_max,temperature_2m_min,weather_code&hourly=temperature_2m,precipitation,visibility,is_day,weather_code&current=temperature_2m,precipitation,weather_code,rain,showers,snowfall,is_day&timezone=America%2FNew_York&forecast_days=3&wind_speed_unit=mph&temperature_unit=fahrenheit&precipitation_unit=inch";
    //Current Temp, Prec, Weather Code, Rain/Shower/Snowfall
    //Hourly Temp, Prec, Visib, UV, Is_Day
    //Daily Temp Min, Temp Max, Weather Code      
const app = express();
app.use(cors());

app.listen(8787, "0.0.0.0", () => console.log("Listening on 8787"));

app.get("/health", (req, res) => {
  res.json({ ok: true});  
});

app.get("/mta", async (req, res) => {
    try {
        const mtaRes = await fetch(MTA_URL_N);
        if(!mtaRes.ok) {
            return res.status(502).json({ Error: "MTA Shi Failed", status: mtaRes.status})
        }
        
        const mtaArrayBuffer = await mtaRes.arrayBuffer();
        const mtaBuffer = Buffer.from(mtaArrayBuffer);
        const mtaFeed = GtfsRealtimeBindings.transit_realtime.FeedMessage.decode(mtaBuffer);
        
        const northbound = [];      //store mins of north  
        const southbound = [];      //same but for south
        const timeNow = Math.floor(Date.now() / 1000);
        
        for (const entity of mtaFeed.entity) {
            if (entity.tripUpdate) {
                const update = entity.tripUpdate.stopTimeUpdate;    //array of predictions
                
                if (!update) continue;
                
                const tripId = entity.tripUpdate.trip.routeId; 
                
                /* const i = tripId.indexOf("_");
                let train;
                if (i === -1) {                     //WHOLE FUNCTION WAS FOR
                    train = tripId;                 //TRIP ID NOT ROUTE OOPS
                } else {
                    train = tripId[i + 1];
                }
                */
               
                for (const x of update) {
                    const stop = x.stopId;
                    
                    let t;
                    
                    if (x.arrival.time != null) {
                        t = x.arrival.time;
                    } else if (x.departure.time != null) {
                        t = x.departure.time;
                    } else {
                        t = null;
                    }
                    
                    if(!t) continue;
                    
                    const minutes = Math.round((t - timeNow) / 60);
                    
                    if(minutes < 0) continue;
                    
                    if (stop === northN) {
                        northbound.push({
                            minutes: minutes,
                            train: tripId
                        });
                    } else if (stop === southN) {
                        southbound.push({
                            minutes: minutes,
                            train: tripId
                        });
                    }
                    
                }
            }
        }
        
        northbound.sort((a, b) => a.minutes - b.minutes);
        southbound.sort((a, b) => a.minutes - b.minutes);
        const north = northbound.slice(0, 5);
        const south = southbound.slice(0, 5);
            
        res.json ({
            north,
            south
        });
        
    } catch (ERR) {
        res.status(500).json({ error: "MTA Proxy error", detail: String(ERR) });
    }
});

app.get("/weather", async (req, res) => {
    try {
        const weatherRes = await fetch(WEATHER_URL); 
        if (!weatherRes.ok) {
            return res.status(502).json({ Error: "MTA Shi Failed", status: weatherRes.status})
        }
        
        const data = await weatherRes.json();
        
        const time = data.hourly.time;                                  //array of time
        const currentHour = data.current.time.slice(0, 13) + ":00";     //fix from 1:34 to 1:00
        const startIndex = time.indexOf(currentHour);
        
        let safeStart = startIndex;
        if (safeStart < 0) {
            safeStart = 0;
        }
        
        const tempArr  = data.hourly.temperature_2m.slice(safeStart);
        const precArr  = data.hourly.precipitation.slice(safeStart);
        const visibArr = data.hourly.visibility.slice(safeStart);
        const dayArr   = data.hourly.is_day.slice(safeStart);
        const codeArr  = data.hourly.weather_code.slice(safeStart);
        
        res.json({
            startIndex: safeStart,            // ADDED (helps ESP32 know where "now" starts)
            current: {
                temp: Math.round(data.current.temperature_2m),
                code: data.current.weather_code,
                prec: data.current.precipitation,
                rain: data.current.rain,
                snow: data.current.snowfall
            },
            hourly: tempArr.map((t, i) => ({
                temp: Math.round(t),
                prec: precArr[i],
                visib: Math.round(visibArr[i]),
                day: dayArr[i],
                code: codeArr[i]                // ADDED
            }))
        });
          
    } catch (ERR) {
        res.status(500).json({ error: "Weather Proxy error", detail: String(ERR) });

    }
})
