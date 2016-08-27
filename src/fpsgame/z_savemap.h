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
    int len = (int)min(file->size(), stream::offset(INT_MAX));
    if(len <= 0 && len > 64<<20) return false;
    uchar *data = new uchar[len];
    if(!data) return false;
    file->seek(0, SEEK_SET);
    file->read(data, len);
    delete file;
    string fname;
    if(mappath[0]) formatstring(fname, "%s/%s.ogz", mappath, mname);
    else formatstring(fname, "%s.ogz", mname);
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
