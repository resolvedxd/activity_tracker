with import <nixpkgs> {};
stdenv.mkDerivation {
    name = "meowfuscator";
    buildInputs = [ pkg-config pkgconf libevdev curl jansson ];
}
