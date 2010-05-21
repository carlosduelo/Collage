
/* Copyright (c) 2010, Stefan Eilemann <eile@eyescale.ch> 
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *  
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef EQFABRIC_CANVAS_H
#define EQFABRIC_CANVAS_H

#include <eq/fabric/types.h>
#include <eq/fabric/visitorResult.h>  // enum
#include <eq/fabric/frustum.h>        // base class
#include <eq/fabric/object.h>         // base class

#include <string>

namespace eq
{
namespace fabric
{
    struct CanvasPath;

    /**
     * A canvas represents a logical 2D projection surface.
     *
     * A canvas consists of one or more Segment, which represent the physical
     * output channels. Segments have a viewport, which defines which part of
     * the logical 2D projection surface they occupy. Segments overlap each
     * other when edge-blending is used, and have gaps for display
     * walls. Passive stereo systems use one segment for each eye pass, so that
     * two segments have the same viewport. Application windows typically use
     * one canvas per Window.
     *
     * A canvas has a Frustum, which is used to compute a sub-frustum for
     * segments which have no frustum specified. This is useful for planar
     * projection systems.
     *
     * A canvas has one ore more layouts, of which one Layout
     * is the active layout, defining the set of logical views currently used to
     * render on the canvas. The layout can be switched at runtime. A canvas
     * with a NULL layout does not render anything, i.e., it is not active.
     */
    template< class CFG, class C, class S, class L >
    class Canvas : public Object, public Frustum
    {
    public:
        typedef std::vector< S* > SegmentVector;
        typedef std::vector< L* > LayoutVector;
        typedef ElementVisitor< C, LeafVisitor< S > > Visitor;
        
        /** @name Data Access */
        //@{
        /** @return the parent config. @version 1.0 */
        CFG*       getConfig()       { return _config; }
        /** @return the parent config. @version 1.0 */
        const CFG* getConfig() const { return _config; }

        /** @return the index of the active layout. @version 1.0 */
        uint32_t getActiveLayoutIndex() const { return _data.activeLayout; }

        /** @return the active layout. @version 1.0 */
        EQFABRIC_EXPORT const L* getActiveLayout() const;

        /** @return the vector of child segments. @version 1.0 */
        const SegmentVector& getSegments() const { return _segments; }        

        /** Find the first segment of a given name. @internal */
        S* findSegment( const std::string& name );

        /** @return the vector of possible layouts. @version 1.0 */
        const LayoutVector& getLayouts() const { return _layouts; }        

        /** Add a new allowed layout to this canvas, can be 0. @internal */
        EQFABRIC_EXPORT void addLayout( L* layout );

        /** @sa Frustum::setWall() */
        EQFABRIC_EXPORT virtual void setWall( const Wall& wall );
        
        /** @sa Frustum::setProjection() */
        EQFABRIC_EXPORT virtual void setProjection( const Projection& );

        /** @sa Frustum::unsetFrustum() */
        EQFABRIC_EXPORT virtual void unsetFrustum();
        //@}

        /** @name Operations */
        //@{
        /** Activate the given layout on this canvas. @version 1.0 */
        EQFABRIC_EXPORT virtual void useLayout( const uint32_t index );

        /** 
         * Traverse this canvas and all children using a canvas visitor.
         * 
         * @param visitor the visitor.
         * @return the result of the visitor traversal.
         * @version 1.0
         */
        EQFABRIC_EXPORT VisitorResult accept( Visitor& visitor );

        /** Const-version of accept(). */
        EQFABRIC_EXPORT VisitorResult accept( Visitor& visitor ) const;

        /** @return true if the layout has changed. @internal */
        bool hasDirtyLayout() const { return getDirty() & DIRTY_LAYOUT; }

        virtual void backup(); //!< @internal
        virtual void restore(); //!< @internal

        void create( S** segment ); //!< @internal
        void release( S* segment ); //!< @internal
        //@}

    protected:
        /** Construct a new Canvas. @internal */
        EQFABRIC_EXPORT Canvas( CFG* config );

        /** Destruct this canvas. @internal */
        EQFABRIC_EXPORT virtual ~Canvas();

        /** @sa Frustum::serialize. @internal */
        EQFABRIC_EXPORT void serialize( net::DataOStream& os, 
                                        const uint64_t dirtyBits );
        /** @sa Frustum::deserialize. @internal */
        EQFABRIC_EXPORT virtual void deserialize( net::DataIStream& is, 
                                                  const uint64_t dirtyBits );

        /** @sa Serializable::setDirty() @internal */
        virtual void setDirty( const uint64_t bits );

        virtual ChangeType getChangeType() const { return UNBUFFERED; }
        virtual void activateLayout( const uint32_t index ) { /* NOP */ }

    private:
        /** The parent config. */
        CFG* const _config;

        struct BackupData
        {
            BackupData() : activeLayout( 0 ) {}

            /** The currently active layout on this canvas. */
            uint32_t activeLayout;
        }
            _data, _backup;

        /** Allowed layouts on this canvas. */
        LayoutVector _layouts;

        /** Child segments on this canvas. */
        SegmentVector _segments;

        union // placeholder for binary-compatible changes
        {
            char dummy[32];
        };

        enum DirtyBits
        {
            DIRTY_LAYOUT    = Object::DIRTY_CUSTOM << 0,
            DIRTY_SEGMENTS  = Object::DIRTY_CUSTOM << 1,
            DIRTY_LAYOUTS   = Object::DIRTY_CUSTOM << 2,
            DIRTY_FRUSTUM   = Object::DIRTY_CUSTOM << 3
        };

        template< class, class, class > friend class Segment;
        void _addSegment( S* segment );
        bool _removeSegment( S* segment );

        /** Deregister this canvas, and all children, from its net::Session.*/
        void _unmap();
        template< class, class, class, class, class, class, class >
        friend class Config;
    };

    template< class CFG, class C, class S, class L >
    std::ostream& operator << ( std::ostream&, const Canvas< CFG, C, S, L >& );
}
}
#endif // EQFABRIC_CANVAS_H
