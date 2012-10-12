(ns slide-explorer.core
  (:import (java.awt AlphaComposite BasicStroke Color Font Graphics Graphics2D
                     Polygon RenderingHints)
           (java.awt.font TextAttribute)
           (javax.swing JFrame JPanel)))

;; possible 
; draggable
; clickable
; hoverable
; rotatable
; resizable
; shearable
;
; gradients
; affine transforms

(defn enable-anti-aliasing
  "Turn on (off) anti-aliasing for a graphics context."
  ([^Graphics g]
    (enable-anti-aliasing g true))
  ([^Graphics g on]
    (let [graphics2d (cast Graphics2D g)]
      (.setRenderingHint graphics2d
                         RenderingHints/KEY_ANTIALIASING
                         (if on
                           RenderingHints/VALUE_ANTIALIAS_ON
                           RenderingHints/VALUE_ANTIALIAS_OFF)))))

(def default-primitive-params
  {:filled false :color Color/BLACK})

(def stroke-caps
  {:butt BasicStroke/CAP_BUTT
   :round BasicStroke/CAP_ROUND
   :square BasicStroke/CAP_SQUARE})

(def stroke-joins
  {:miter BasicStroke/JOIN_MITER
   :round BasicStroke/JOIN_ROUND
   :bevel BasicStroke/JOIN_BEVEL})

(defn set-style [g2d {:keys [alpha color stroke]}]
  (doto g2d
    (.setComposite (if (or (not alpha) 
                           (= alpha 1))
                     AlphaComposite/Src
                     (AlphaComposite/getInstance AlphaComposite/SRC_ATOP alpha)))
    (.setColor (if color color Color/BLACK))
    (.setStroke (let [{:keys [width cap join miter-limit dashes dash-phase]
                       :or {width 1.0 cap :square
                            join :miter miter-limit 10.0
                            dashes [] dash-phase 0.0}} stroke
                      cap-code (stroke-caps cap)
                      join-code (stroke-joins join)
                      dashes-array (float-array dashes)]
                  (if-not (empty? dashes)
                    (BasicStroke. width cap-code join-code miter-limit
                                  dashes-array dash-phase)
                    (BasicStroke. width cap-code join-code miter-limit))))))

(defn- -? [a b]
  (when (and a b)
    (- a b)))

(defn- +? [a b]
  (when (and a b)
    (+ a b)))

(defn- half? [a]
  (when a
    (/ a 2)))

(defn- twice? [a]
  (when a
    (* a 2)))

(defn complete-coordinates
  [{:keys [l left t top r right b bottom
           x y
           w width h height]
    :as the-map}]
  (let [l (or l left) t (or t top) r (or r right) b (or b bottom)
        w (or w width (-? r l) (twice? (-? x l)) (twice? (-? r x)))
        h (or h height (-? b t) (twice? (-? y t)) (twice? (-? b y)))
        l (or l (-? r w) (-? x (half? w)))
        t (or t (-? b h) (-? y (half? h)))
        r (or r (+? l w) (+? x (half? w)))
        b (or b (+? t h) (+? y (half? h)))
        x (or x (+? l (half? w)))
        y (or y (+? t (half? h)))]
    (merge the-map {:w w :h h :l l :t t :r r :b b :x x :y y})))
   
(defmulti draw-primitive (fn [g2d shape params] shape))

(defmethod draw-primitive :primitive-arc
  [g2d shape {:keys [l t w h filled start-angle arc-angle]}]
  (if filled
    (.fillArc g2d l t w h start-angle arc-angle)
    (.drawArc g2d l t w h start-angle arc-angle)))

(defmethod draw-primitive :primitive-ellipse
  [g2d shape {:keys [l t w h filled]}]
  (if filled
    (.fillOval g2d l t w h)
    (.drawOval g2d l t w h)))

(defmethod draw-primitive :primitive-image
  [g2d shape {:keys [image l t w h]}]
  (.drawImage g2d image
              l t
              (or w (.getWidth image))
              (or h (.getHeight image)) nil))

(defmethod draw-primitive :primitive-line
  [g2d shape {:keys [l t r b filled]}]
  (.drawLine g2d l t r b))

(defmethod draw-primitive :primitive-polygon
  [g2d shape {:keys [vertices filled closed]}]
  (let [xs (int-array (map :x vertices))
        ys (int-array (map :y vertices))
        n (count vertices)]
    (if filled
      (.fillPolygon g2d xs ys n)
      (if closed
        (.drawPolygon g2d xs ys n)))
        (.drawPolyline g2d xs ys n)))

(defmethod draw-primitive :primitive-rect
  [g2d shape {:keys [l t w h filled]}]
  (if filled
    (.fillRect g2d l t w h)
    (.drawRect g2d l t w h)))

(defmethod draw-primitive :primitive-round-rect
  [g2d shape {:keys [l t w h filled arc-radius arc-width arc-height]}]
  (if filled
    (.fillRoundRect g2d l t w h
                    (or arc-radius arc-width)
                    (or arc-radius arc-height))
    (.drawRoundRect g2d l t w h
                    (or arc-radius arc-width)
                    (or arc-radius arc-height))))

(defn draw-string-center
  "Draw a string centered at position x,y."
  [^Graphics2D graphics ^String text x y]
  (let [context (.getFontRenderContext graphics)
        height (.. graphics
                   getFont (createGlyphVector context text)
                   getVisualBounds
                   getHeight)
        width (.. graphics
                  getFontMetrics
                  (getStringBounds text graphics)
                  getWidth)]
    (.drawString graphics text
                 (float (- x (/ width 2)))
                 (float (+ y (/ height 2))))))

(defmethod draw-primitive :primitive-text
  [g2d shape  {:keys [text x y font]}]
  (let [{:keys [name bold italic underline strikethrough size]} font
        style (bit-or (if bold Font/BOLD 0) (if italic Font/ITALIC 0))
        font1 (Font. name style size)
        attributes (.getAttributes font1)]
    (doto attributes
      (.put TextAttribute/STRIKETHROUGH
            (if strikethrough TextAttribute/STRIKETHROUGH_ON false))
      (.put TextAttribute/UNDERLINE
            (if underline TextAttribute/UNDERLINE_ON -1)))
    (let [font2 (Font. attributes)]
      (.setFont g2d font2)
      (draw-string-center g2d text x y))))

(defn draw-primitives [g2d items]
  (enable-anti-aliasing g2d)
  (doseq [[type params inner] items]
    (println type params inner)
    (set-style g2d params)
    (draw-primitive g2d type (complete-coordinates params))
    ))

(defn paint-canvas-graphics [^Graphics graphics data]
  (draw-primitives graphics data))

(defn canvas [reference]
  (let [panel (proxy [JPanel] []
                (paintComponent [^Graphics graphics]
                                (proxy-super paintComponent graphics)
                                (paint-canvas-graphics graphics @reference)))]
    (add-watch reference panel (fn [_ _ _ _]
                                 (.repaint panel)))
    panel))

;; test

(def grafix (atom nil))

(defn canvas-frame [reference]
  (let [panel (canvas reference)]
    (doto (JFrame. "canvas")
      (.. getContentPane (add panel))
      (.setBounds 10 10 500 500)
      .show)
    panel))

(defn demo-atom []
  (canvas-frame grafix))

(defn demo-animation [reference]
  (dotimes [i 200]
  (Thread/sleep 30)
  (swap! reference (fn [data]
                     (-> data
                         (assoc-in [0 :params :w] (+ i 100))
                         (assoc-in [0 :params :h] (+ i 100)))
                     ))))

(reset! grafix 
  [
   [:primitive-round-rect
    {:l 20 :t 10 :w 300 :h 300
             :arc-radius 50
             :filled true :color Color/PINK}]
   [:primitive-ellipse
    {:l 25 :t 15 :w 110 :h 90
             :filled true :color Color/YELLOW}]
   [:primitive-round-rect
    {:l 20 :t 10 :w 300 :h 300
             :arc-width 50 :arc-height 50
             :filled false :color Color/DARK_GRAY
             :stroke {:width 5
                      :dashes [10 10] :dash-phase 0}}]
   [:primitive-polygon
    {:vertices [{:x 100 :y 100}
                        {:x 50 :y 150}
                        {:x 50 :y 220}
                        {:x 160 :y 250}]
             :filled false
             :closed false
             :color Color/ORANGE
             :alpha 0.8
             :stroke {:width 25
                      :dashes [20 3 10 3 5 3]
                      :cap :butt
                      :join :bevel
                      :miter-limit 10.0}}]
   [:primitive-text
    {:x 180 :y 120 :text "TEST"
             :color Color/BLUE
             :alpha 0.5
             :font {:name "Arial"
                    :bold true
                    :italic false
                    :underline true
                    :strikethrough false
                    :size 100}}]
   [:primitive-line
    {:x 180 :y 120 :w 0 :h 150 :color Color/RED
             :stroke {:width 10 :cap :round}
             :alpha 0.7}]
   [:primitive-line
    {:x 180 :y 120 :w 30 :h 0 :color Color/GREEN
             :alpha 0.6
             :stroke {:width 4}}]
   [:primitive-arc
    {:l 60 :t 30 :w 100 :h 100
             :start-angle 30 :arc-angle 100 :color Color/GREEN
             :filled true
             :alpha 0.7}]
   ])


