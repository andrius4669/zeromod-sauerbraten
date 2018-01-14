#ifdef Z_SAVEMAP_H
#error "already z_savemap.h"
#endif
#define Z_SAVEMAP_H

#ifndef Z_SERVCMD_H
#error "want z_servcmd.h"
#endif

#ifndef Z_LOADMAP_H
#error "want z_loadmap.h"
#endif

ICOMMAND(serversavemap, "i", (int *i), z_enable_command("savemap", *i!=0));

bool z_savemap(const char *mname, stream *&file = mapdata)
{
    if(!file) return false;

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

    int len = (int)min(file->size(), stream::offset(INT_MAX));
    if(len <= 0 && len > 64<<20) return false;
    uchar *data = new uchar[len];
    if(!data) return false;
    file->seek(0, SEEK_SET);
    file->read(data, len);
    delete file;

    file = openrawfile(path(fname), "w+b");
    if(file)
    {
        file->write(data, len);
        delete[] data;
        return true;
    }
    else
    {
        file = opentempfile("mapdata", "w+b");
        if(file) file->write(data, len);
        delete[] data;
        return false;
    }
}

void z_servcmd_savemap(int argc, char **argv, int sender)
{
    const char *mname = argc >= 2 ? argv[1] : smapname;
    if(!mname || !mname[0]) { sendf(sender, 1, "ris", N_SERVMSG, "please specify map name"); return; }
    if(z_savemap(mname)) sendservmsgf("[map \"%s\" saved]", mname);
    else sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("failed to save map: %s", mname));
}
SCOMMANDA(savemap, ZC_DISABLED | PRIV_ADMIN, z_servcmd_savemap, 1);
