import GtfsRealtimeBindings from "gtfs-realtime-bindings";
import { Analytics } from "@vercel/analytics/next"

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

  // comes from ?path=health | mta | weather
  const route = url.searchParams.get("path") || "";

  if (route === "health") {
    res.statusCode = 200;
    res.setHeader("Content-Type", "application/json");
    return res.end(JSON.stringify({ ok: true }));
  }

  if (route === "mta") {
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

      const northbound = [];
      const southbound = [];
      const timeNow = Math.floor(Date.now() / 1000);

      for (const entity of mtaFeed.entity) {
        if (!entity.tripUpdate) continue;

        const update = entity.tripUpdate.stopTimeUpdate;
        if (!update) continue;

        const tripId = entity.tripUpdate.trip.routeId;

        for (const x of update) {
          const stop = x.stopId;

          let t;
          if (x.arrival?.time != null) t = x.arrival.time;
          else if (x.departure?.time != null) t = x.departure.time;
          else continue;

          const minutes = Math.round((t - timeNow) / 60);
          if (minutes < 0) continue;

          if (stop === northN) {
            northbound.push({ minutes, train: tripId });
          } else if (stop === southN) {
            southbound.push({ minutes, train: tripId });
          }
        }
      }

      northbound.sort((a, b) => a.minutes - b.minutes);
      southbound.sort((a, b) => a.minutes - b.minutes);

      res.statusCode = 200;
      res.setHeader("Content-Type", "application/json");
      return res.end(
        JSON.stringify({
          north: northbound.slice(0, 5),
          south: southbound.slice(0, 5),
        })
      );
    } catch (ERR) {
      res.statusCode = 500;
      res.setHeader("Content-Type", "application/json");
      return res.end(
        JSON.stringify({ error: "MTA Proxy error", detail: String(ERR) })
      );
    }
  }

  if (route === "weather") {
    try {
      const weatherRes = await fetch(WEATHER_URL);

      if (!weatherRes.ok) {
        res.statusCode = 502;
        res.setHeader("Content-Type", "application/json");
        return res.end(
          JSON.stringify({ Error: "Weather Shi Failed", status: weatherRes.status })
        );
      }

      const data = await weatherRes.json();

      const time = data.hourly.time;
      const currentHour = data.current.time.slice(0, 13) + ":00";
      const startIndex = time.indexOf(currentHour);

      let safeStart = startIndex;
      if (safeStart < 0) safeStart = 0;

      res.statusCode = 200;
      res.setHeader("Content-Type", "application/json");
      return res.end(
        JSON.stringify({
          startIndex: safeStart,
          current: {
            temp: Math.round(data.current.temperature_2m),
            code: data.current.weather_code,
            prec: data.current.precipitation,
            rain: data.current.rain,
            snow: data.current.snowfall,
          },
          hourly: data.hourly.temperature_2m
            .slice(safeStart)
            .map((t, i) => ({
              temp: Math.round(t),
              prec: data.hourly.precipitation[safeStart + i],
              visib: Math.round(data.hourly.visibility[safeStart + i]),
              day: data.hourly.is_day[safeStart + i],
              code: data.hourly.weather_code[safeStart + i],
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
  return res.end(JSON.stringify({ error: "Not Found", route }));
}
