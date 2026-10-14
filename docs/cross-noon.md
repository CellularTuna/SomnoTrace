# Sessions That Cross Noon

Therapy that runs through midday — a late morning lie-in, a nap that starts before 12:00 and ends after — is recorded by SomnoTrace as **one continuous session**. The AS10 and AS11 cannot do this: they cut a session at noon and write the two halves to different days.

This is deliberate, and it is why SomnoTrace records the stream itself rather than relying on the machine's own summary.

**Why noon at all?** ResMed's file format organises therapy into noon-to-noon days, so a session that begins before 12:00 belongs to the *previous* date. Sleeping from 23:00 Monday to 07:00 Tuesday is one "Monday" night, which is what you want. A session that crosses the noon boundary is the awkward case that convention creates.

**What you will see**

| Where | How a cross-noon session appears |
| :--- | :--- |
| SomnoTrace dashboard | One session, with its true start and end |
| OSCAR | One session, with its true start and end |
| Exported `BRP`/`PLD`/`EVE`/`CSL` EDFs | One recording, not split |

OSCAR takes a session's extent from the recording files themselves, not from the `MaskOn`/`MaskOff` pairs in `STR.edf` — those pairs only group files into sessions and mark a day as having usable data. So a session running 06:00–13:00 displays as one seven-hour session.

**The boundary detail, for anyone reading the STR file**

`MaskOn`/`MaskOff` are minutes from noon and the format declares a maximum of 1440. A cross-noon session produces a window that runs past that. SomnoTrace **clamps** such a window to `[0, 1440]` rather than discarding it, so the day keeps a valid STR record — and therefore its settings: mode, pressures, EPR, and the day's statistics. A dropped record would leave the night importable but with none of that attached.

**Limits of the above.** OSCAR's behaviour here is read from its source (`resmed_loader.cpp`), not measured against a running copy. SleepHQ's importer is closed, so nothing is claimed for it.
