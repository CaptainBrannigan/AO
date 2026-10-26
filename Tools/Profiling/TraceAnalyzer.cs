using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

public static class TraceAnalyzer
{
    struct Ev { public string Timer; public double Start; public double End; public double DurMs; public int Depth; }

    static IEnumerable<Ev> Read(string path, double skip)
    {
        using (var r = new StreamReader(path))
        {
            r.ReadLine();
            string line;
            while ((line = r.ReadLine()) != null)
            {
                // ThreadName,TimerName,StartTime,EndTime,Duration,Depth  (timer names may contain commas; parse from the right)
                Ev e;
                try
                {
                    int c5 = line.LastIndexOf(',');
                    int c4 = line.LastIndexOf(',', c5 - 1);
                    int c3 = line.LastIndexOf(',', c4 - 1);
                    int c2 = line.LastIndexOf(',', c3 - 1);
                    int c1 = line.IndexOf(',');
                    e.Start = double.Parse(line.Substring(c2 + 1, c3 - c2 - 1), CultureInfo.InvariantCulture);
                    e.Timer = line.Substring(c1 + 1, c2 - c1 - 1);
                    e.End = double.Parse(line.Substring(c3 + 1, c4 - c3 - 1), CultureInfo.InvariantCulture);
                    e.DurMs = double.Parse(line.Substring(c4 + 1, c5 - c4 - 1), CultureInfo.InvariantCulture) * 1000.0;
                    e.Depth = int.Parse(line.Substring(c5 + 1), CultureInfo.InvariantCulture);
                }
                catch (Exception) { continue; }
                if (e.Start < skip) continue;
                yield return e;
            }
        }
    }

    static string Summ(string label, List<double> v)
    {
        if (v.Count == 0) return string.Format("{0,-36} (no samples)", label);
        var s = v.OrderBy(x => x).ToList();
        int n = s.Count;
        return string.Format("{0,-36} n={1,6}  avg={2,6:N2}  p50={3,6:N2}  p95={4,6:N2}  p99={5,6:N2}  max={6,7:N2} ms",
            label, n, s.Average(), s[n / 2], s[(int)(n * 0.95)], s[Math.Min(n - 1, (int)(n * 0.99))], s[n - 1]);
    }

    public static string Frames(string gameFile, double skip, double budgetMs, out int frameCount)
    {
        var ticks = new List<Ev>(); var waits = new List<Ev>();
        foreach (var e in Read(gameFile, skip))
        {
            if (e.Timer == "FEngineLoop::Tick") ticks.Add(e);
            else if (e.Timer == "xrWaitFrame") waits.Add(e);
        }
        ticks.Sort((a, b) => a.Start.CompareTo(b.Start)); waits.Sort((a, b) => a.Start.CompareTo(b.Start));
        var work = new List<double>(); var intervals = new List<double>(); var waitL = new List<double>(); var tickL = new List<double>();
        int wi = 0;
        for (int i = 0; i < ticks.Count; i++)
        {
            double ws = 0;
            while (wi < waits.Count && waits[wi].Start < ticks[i].End) { if (waits[wi].Start >= ticks[i].Start) ws += waits[wi].DurMs; wi++; }
            work.Add(ticks[i].DurMs - ws); tickL.Add(ticks[i].DurMs);
            if (i > 0) intervals.Add((ticks[i].Start - ticks[i - 1].Start) * 1000.0);
        }
        foreach (var w in waits) waitL.Add(w.DurMs);
        frameCount = ticks.Count;
        double dur = ticks[ticks.Count - 1].End - ticks[0].Start;
        int missed = intervals.Count(x => x > budgetMs * 1.5);
        var sb = new StringBuilder();
        sb.AppendLine(string.Format("Capture: {0:N1} s, {1} frames, {2:N1} fps avg", dur, ticks.Count, ticks.Count / dur));
        sb.AppendLine(Summ("Frame interval", intervals));
        sb.AppendLine(string.Format("Frames over {0:N1} ms: {1} of {2} ({3:P2})", budgetMs * 1.5, missed, intervals.Count, (double)missed / Math.Max(1, intervals.Count)));
        sb.AppendLine(Summ("GameThread tick (incl. xrWaitFrame)", tickL));
        sb.AppendLine(Summ("  xrWaitFrame", waitL));
        sb.AppendLine(Summ("  GameThread actual work", work));
        return sb.ToString();
    }

    public static string TimerStats(string file, double skip, string[] timers)
    {
        var d = new Dictionary<string, List<double>>();
        foreach (var t in timers) d[t] = new List<double>();
        foreach (var e in Read(file, skip)) { List<double> l; if (d.TryGetValue(e.Timer, out l)) l.Add(e.DurMs); }
        var sb = new StringBuilder();
        foreach (var t in timers) sb.AppendLine(Summ(t, d[t]));
        return sb.ToString();
    }

    // Aggregate a timer's time by its ancestor path (events are exported in start order per thread).
    public static string Paths(string file, double skip, int frames, string target, int top)
    {
        var stack = new string[64];
        var agg = new Dictionary<string, double>();
        foreach (var e in Read(file, skip))
        {
            if (e.Depth < 64) stack[e.Depth] = e.Timer;
            if (e.Timer != target) continue;
            var parts = new List<string>();
            for (int i = 0; i < e.Depth && i < 64; i++) parts.Add(stack[i]);
            string path = string.Join(" > ", parts);
            double v; agg.TryGetValue(path, out v); agg[path] = v + e.DurMs;
        }
        var sb = new StringBuilder();
        foreach (var kv in agg.OrderByDescending(x => x.Value).Take(top))
            sb.AppendLine(string.Format("  {0,7:N3} ms/frame  {1}", kv.Value / frames, kv.Key));
        return sb.ToString();
    }

    public static string Breakdown(string file, double skip, int frames, int depthMin, int depthMax, int top)
    {
        var sums = new Dictionary<int, Dictionary<string, double>>();
        for (int k = depthMin; k <= depthMax; k++) sums[k] = new Dictionary<string, double>();
        foreach (var e in Read(file, skip))
        {
            if (e.Depth < depthMin || e.Depth > depthMax) continue;
            var m = sums[e.Depth]; double v; m.TryGetValue(e.Timer, out v); m[e.Timer] = v + e.DurMs;
        }
        var sb = new StringBuilder();
        for (int k = depthMin; k <= depthMax; k++)
        {
            sb.AppendLine("  -- depth " + k);
            foreach (var kv in sums[k].OrderByDescending(x => x.Value).Take(top))
                sb.AppendLine(string.Format("  {0,-60} {1,7:N3} ms/frame", kv.Key.Length > 60 ? kv.Key.Substring(0, 60) : kv.Key, kv.Value / frames));
        }
        return sb.ToString();
    }
}
