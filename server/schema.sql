CREATE TABLE logs (
  id SERIAL PRIMARY KEY,
  time INT,
  interval INT,
  platform INT,
  left_clicks INT,
  right_clicks INT,
  mouse_move FLOAT,
  keypresses INT
);
