import GtfsRealtimeBindings from "gtfs-realtime-bindings";

const northA = "A28N";
const southA = "A28S";
const northN = "R17N";
const southN = "R17S";

const MTA_URL_A =
  "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-ace";
const MTA_URL_N =
  "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-nqrw";

const LAT = 40.7506;
const LON = -73.9935;
const WEATHER_URL =
  "https://api.open-meteo.com/v1/forecast?latitude=40.7506&longitude=-73.9935&daily=temperature_2m_max,temperature_2m_min,weather_code&hourly=temperature_2m,precipitation,visibility,is_day,weather_code&current=temperature_2m,precipitation,weather_code,rain,showers,snowfall,is_day&timezone=America%2FNew_York&forecast_days=3&wind_speed_unit=mph&temperature_unit=fahrenheit&precipitation_unit=inch";
//Current Temp, Prec, Weather Code, Rain/Shower/Snowfall
//Hourly Temp, Prec, Visib, UV, Is_Day
//Daily Temp Min, Temp Max, Weather Code

export default async function handler(req, res) {
  console.log("HIT", req.method, req.url);
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Access-Control-Allow-Methods", "GET,OPTIONS");
  res.setHeader("Access-Control-Allow-Headers", "Content-Type");

  if (req.method === "OPTIONS") {
    res.statusCode = 204;
    return res.end();
  }

  const url = new URL(req.url, `http://${req.headers.host}`);

  // this will be "health", "weather", "mta", etc.
  const route = url.searchParams.get("path") || "";

  if (path === "health") {
    res.statusCode = 200;
    res.setHeader("Content-Type", "application/json");
    return res.end(JSON.stringify({ ok: true }));
  }

  if (path === "mta") {
    try {
      const mtaRes = await fetch(MTA_URL_N);
      if (!mtaRes.ok) {
        res.statusCode = 502;
        res.setHeader("Content-Type", "application/json");
        return res.end(
          JSON.stringify({ Error: "MTA Shi Failed", status: mtaRes.status })
        );
      }

      const mtaArrayBuffer = await mtaRes.arrayBuffer();
      const mtaBuffer = Buffer.from(mtaArrayBuffer);
      const mtaFeed =
        GtfsRealtimeBindings.transit_realtime.FeedMessage.decode(mtaBuffer);

      const northbound = []; //store mins of north
      const southbound = []; //same but for south
      const timeNow = Math.floor(Date.now() / 1000);

      for (const entity of mtaFeed.entity) {
        if (entity.tripUpdate) {
          const update = entity.tripUpdate.stopTimeUpdate; //array of predictions

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

            if (!t) continue;

            const minutes = Math.round((t - timeNow) / 60);

            if (minutes < 0) continue;

            if (stop === northN) {
              northbound.push({
                minutes: minutes,
                train: tripId,
              });
            } else if (stop === southN) {
              southbound.push({
                minutes: minutes,
                train: tripId,
              });
            }
          }
        }
      }

      northbound.sort((a, b) => a.minutes - b.minutes);
      southbound.sort((a, b) => a.minutes - b.minutes);
      const north = northbound.slice(0, 5);
      const south = southbound.slice(0, 5);

      res.statusCode = 200;
      res.setHeader("Content-Type", "application/json");
      return res.end(JSON.stringify({ north, south }));
    } catch (ERR) {
      res.statusCode = 500;
      res.setHeader("Content-Type", "application/json");
      return res.end(
        JSON.stringify({ error: "MTA Proxy error", detail: String(ERR) })
      );
    }
  }

  if (path === "weather") {
    try {
      const weatherRes = await fetch(WEATHER_URL);
      if (!weatherRes.ok) {
        res.statusCode = 502;
        res.setHeader("Content-Type", "application/json");
        return res.end(
          JSON.stringify({ Error: "MTA Shi Failed", status: weatherRes.status })
        );
      }

      const data = await weatherRes.json();

      const time = data.hourly.time; //array of time
      const currentHour = data.current.time.slice(0, 13) + ":00"; //fix from 1:34 to 1:00
      const startIndex = time.indexOf(currentHour);

      let safeStart = startIndex;
      if (safeStart < 0) {
        safeStart = 0;
      }

      const tempArr = data.hourly.temperature_2m.slice(safeStart);
      const precArr = data.hourly.precipitation.slice(safeStart);
      const visibArr = data.hourly.visibility.slice(safeStart);
      const dayArr = data.hourly.is_day.slice(safeStart);
      const codeArr = data.hourly.weather_code.slice(safeStart);

      res.statusCode = 200;
      res.setHeader("Content-Type", "application/json");
      return res.end(
        JSON.stringify({
          startIndex: safeStart, // ADDED (helps ESP32 know where "now" starts)
          current: {
            temp: Math.round(data.current.temperature_2m),
            code: data.current.weather_code,
            prec: data.current.precipitation,
            rain: data.current.rain,
            snow: data.current.snowfall,
          },
          hourly: tempArr.map((t, i) => ({
            temp: Math.round(t),
            prec: precArr[i],
            visib: Math.round(visibArr[i]),
            day: dayArr[i],
            code: codeArr[i], // ADDED
          })),
        })
      );
    } catch (ERR) {
      res.statusCode = 500;
      res.setHeader("Content-Type", "application/json");
      return res.end(
        JSON.stringify({ error: "Weather Proxy error", detail: String(ERR) })
      );
    }
  }

  res.statusCode = 404;
  res.setHeader("Content-Type", "application/json");
  return res.end(JSON.stringify({ error: "Not Found" }));
}