
### Polyrhythmic Illumination

Project Members: Gary Kirkham, Rob Gorbet, Shaan Armaan Sawrup, and Matilda Pawlega.

---

Fifteen glowing tubes pulse, drift, and collide, sculpting light and sound into living rhythms. Part puzzle, part engineered light play, the installation is mesmerizing to see and hear.

A polyrhythmic light and sound installation featuring an equally spaced array of fifteen vertical LED tubes, each six feet tall. Within each tube, a glowing sphere rises and falls, each moving at a slightly different tempo. Every period triggers a musical note specific to its shaft.

As with minimalist pioneer Sol LeWitt, simple rules generate complex and beautiful forms. The piece begins in unison: all fifteen lights strike together, forming a rich chord. Gradually, the rhythms drift apart—first arpeggios, then counterpoint, then moments of apparent musical and visual chaos—before resolving into new harmonies and eventually returning to synchronicity. The system continually evolves, paradoxically growing both more complex and more ordered, finally returning to the original chord where the cycle begins again.

The work is rooted in an interest in patterns and their connection to our search for meaning. Polyrhythms mirror the cyclical nature of history—past, present, and what often feels like a repeating future. The installation embodies these cycles, inviting reflection on what it means to drift apart and come back together, both literally and figuratively.

In a time when the world feels especially chaotic, *Polyrhythmic Illuminations* guides audiences through phases of discord and synchronization. Its slow, meditative unfolding acknowledges disorder while offering the possibility of realignment, renewed togetherness, and a future shaped by synchronicity rather than fragmentation. The work also acts as a provocation: what do we want our future together to be like? Will we strive for concordance or embrace chaos—and can one exist without the other?

---

### System Infrastructure

Each light tube is controlled by a microcontroller housed in a custom **3D-printed enclosure**. The system uses Wemos D1 Mini microcontrollers to run the behaviour of each tube.

Lighting is driven by addressable WS2812B LED strips, programmed in C++ using the Arduino IDE and the FastLED library to control colour, brightness, and motion.

A central control computer running MAX MSP 8 manages the audio and produces polyrhythms, sending colour and timing (period) information wirelessly over a local Wi-Fi network to each tube. This distributed architecture synchronizes the motion of light and musical notes across all fifteen elements while allowing subtle tempo differences that generate the evolving polyrhythmic patterns.

---

### Installation

The piece comprises fifteen 6-inch diameter light tubes installed free-standing on heavy bases in a linear array approximately 6–8 inches apart (about 18 inches deep and 15 feet wide). A rented audio system with two amplified three-way speakers augments the visual installation. Total installed footprint is approximately **3 ft × 20 ft**.

A local Wi-Fi LAN provides communication between components, and power supplies drive the light tubes and the central control computer. The installation requires **one 15A 120VAC circuit**.

The work can be installed indoors or outdoors (with a preference for outdoor settings such as parks or waterfronts, where the contrast between technology and nature enhances the experience). It has proven weatherproof during Cambridge’s Unsilent Night festival. The updated design uses fully cylindrical tubes, so the installation is visible from all sides.

The work is most resolved when viewed from **8–30 feet** away on a flat, level surface. Close viewing and gentle touching are welcome, and an artist will be present during the exhibition to ensure the installation remains safe.

While the installation does not require a specific neighbourhood or land connection, a natural setting such as a park or waterfront is preferred. The juxtaposition between the natural environment and the minimalist technological aesthetic enhances the meditative quality of the work.

From an accessibility perspective, the experience is limited only by the accessibility of the site. While visually or auditory-impaired visitors may not experience all aspects of the installation, its multiple sensory modes allow a wide range of audiences to engage meaningfully with the work.

---

### Audience Experience

Audiences at previous installations of *Polyrhythmic Illuminations* have described the work as mesmerizing and meditative. Complex patterns emerging from simple behaviours resonate on a visceral level.

The installation encourages community interaction. Families and children share photos on social media, and visitors often discuss the mathematics and rhythms behind the patterns. As crowds thin later in the night, curious visitors may be invited to control aspects of the system’s behaviour.

When the drifting rhythms finally resolve into synchronicity, assembled audiences often anticipate the moment and cheer together. The duration of the re-phasing cycle—the time it takes for the system to return to unison—is programmable, allowing these moments to be timed to audience flow.

Individuals frequently watch attentively for **15–20 minutes**, immersed in the evolving patterns and the polyrhythmic musical composition, while the installation also remains engaging for passersby.

---

### Artists
Based in KW, Ontario, Canada.

**Gary Kirkham – Lead Artist**
[https://gazzkirkham.wixsite.com/playwright](https://gazzkirkham.wixsite.com/playwright)

Gary Kirkham is a multidisciplinary artist based in Cambridge. His work spans video, installation, and theatre, often exploring immersive, collaborative, and cross-cultural experiences.
He has collaborated with Krzysztof Wodiczko (CAFKA) and Liggy Haugue – *Experiment for a Closing* (The Idea Exchange). His video installations include *Blue Memos* (Numus Festival), *Dancing Within Our Worlds* with Lidia Kopinan (Russia) and Gisel Vergara (Mexico), *Poesy* (Canada), and Yaser Khaseb (Iran). Outdoor works include *NightShift* and *UnSilent Night*.
In theatre, his plays, including *Falling: A Wake*, *Pearl Gidley*, and *Queen Milli of Galt* have been performed internationally in over 150 theatres and translated into French, Italian, and Arabic. His numerous collaborations with MT Space, including *The Last 15 Seconds*, continue to explore immersive sound, light, and movement.

**Rob Gorbet – Artist and Engineer**
[www.gorbetddesign.com](http://www.gorbetddesign.com)

Rob Gorbet is a mechatronics engineer and technology artist who has exhibited collaborative works around the world. He collaborates with artists and architects to create unforgettable experiences for audiences. His work *Hylozoic Ground*, with Philip Beesley and the Living Architecture Systems Group, represented Canada at the Venice Biennale in 2010. He has also been a collaborator on two prior Nuit Blanche works: *Aurora* (2010) and *Ocean* (2016).

**Matilda Pawlega – Electronics Artist**
Instagram: [https://www.instagram.com/anoaeyto/](@anoaeyto)

**Shaan Sawrup – Audio Composition**
[https://opalsounds.bandcamp.com/](https://opalsounds.bandcamp.com/)

Matilda Pawlega and Shaan Sawrup are students at the University of Waterloo and members of the PRISM Collective, a boundary-breaking Waterloo student design team encouraging new forms of design and expression among STEM-oriented students.





