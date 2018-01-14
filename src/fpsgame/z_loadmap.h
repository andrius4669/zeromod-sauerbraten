#ifdef Z_LOADMAP_H
#error "already z_loadmap.h"
#endif
#define Z_LOADMAP_H

#ifndef Z_SERVCMD_H
#error "want z_servcmd.h"
#endif

SVAR(mappath, "packages/base");

bool z_loadmap(const char *mname, stream *&data = mapdata)
{
    string fname, vname;
    validmapname(vname, mname, NULL, "");
    // if mapname was modified it means it was not OK
    size_t vlen = strlen(vname);
    if(vlen != strlen(mname)) return false;
    size_t plen = strlen(mappath);
    if(plen+vlen+4 > sizeof(fname)-1) return false; // too long
    memcpy(fname, mappath, plen);
    int slash = 0;
    if(plen && mappath[plen-1] != '/')
    {
        if(plen+1+vlen+4 > sizeof(fname)-1) return false; // too long
        fname[plen] = '/';
        slash = 1;
    }
    memcpy(&fname[plen+slash], vname, vlen);
    memcpy(&fname[plen+slash+vlen], ".ogz", 4);
    fname[plen+slash+vlen+4] = '\0';

    stream *map = openrawfile(path(fname), "rb");
    if(!map) return false;
    stream::offset len = map->size();
    if(len <= 0 || len > 16<<20) { delete map; return false; }
    DELETEP(data);
    data = map;
    return true;
}

void z_servcmd_loadmap(int argc, char **argv, int sender)
{
    const char *mname = argc >= 2 ? argv[1] : smapname;
    if(!mname || !mname[0]) { sendf(sender, 1, "ris", N_SERVMSG, "please specify map name"); return; }
    if(z_loadmap(mname)) sendservmsgf("[map \"%s\" loaded]", mname);
    else sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("failed to load map: %s", mname));
}
SCOMMANDA(loadmap, PRIV_MASTER, z_servcmd_loadmap, 1);

void z_servcmd_listmaps(int argc, char **argv, int sender)
{
    vector<char *> files;
    vector<char> line;
    listfiles(mappath, "ogz", files);
    files.sort();
    sendf(sender, 1, "ris", N_SERVMSG, files.length() ? "server map files:" : "server has no map files");
    for(int i = 0; i < files.length();)
    {
        line.setsize(0);
        for(int j = 0; i < files.length() && j < 5; i++, j++)
        {
            if(j) line.add(' ');
            line.put(files[i], strlen(files[i]));
        }
        line.add(0);
        sendf(sender, 1, "ris", N_SERVMSG, line.getbuf());
    }
    files.deletearrays();
}
SCOMMANDA(listmaps, PRIV_MASTER, z_servcmd_listmaps, 1);
